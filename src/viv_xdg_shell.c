#include <pixman-1/pixman.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_output_damage.h>

#include "viv_config_support.h"
#include "viv_damage.h"
#include "viv_xdg_shell.h"
#include "viv_seat.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_wlr_surface_tree.h"
#include "viv_workspace.h"
#include "viv_xdg_popup.h"
#include "viv_output.h"

/// Return true if the view looks like it should be floating.
static bool guess_should_be_floating(struct viv_view *view) {
    if (view->xdg_surface->toplevel->parent) {
        return true;
    }

    struct wlr_xdg_toplevel_state *current = &view->xdg_surface->toplevel->current;
    if (!current->min_width || !current->min_height) {
        return false;
    }
    return ((current->min_width == current->max_width) || (current->min_height == current->max_height));
}

static void add_xdg_view_global_coords(void *view_pointer, int *x, int *y) {
    struct viv_view *view = view_pointer;
    *x += view->x;
    *y += view->y;
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;

    wl_list_remove(&view->workspace_link);

    uint32_t view_name_len = 200;
    char view_name[view_name_len];
    viv_view_get_string_identifier(view, view_name, view_name_len);

    struct wlr_xdg_toplevel_requested *requested = &view->xdg_surface->toplevel->requested;
    bool fullscreen_request_denied = false;

    if (requested->fullscreen && requested->fullscreen_output) {
        struct viv_output *requested_output = viv_output_of_wlr_output(view->server, requested->fullscreen_output);
        if (!requested_output) {
            wlr_log(WLR_ERROR, "Couldn't find requested fullscreen output for \"%s\"", view_name);
            fullscreen_request_denied = true;
        } else {
            if (requested_output->current_workspace->fullscreen_view) {
                wlr_log(WLR_DEBUG, "Output requested by \"%s\" already has a fullscreen view", view_name);
                fullscreen_request_denied = true;
            } else {
                wlr_log(WLR_DEBUG,"Moving \"%s\" to another workspace before attempting to go fullscreen", view_name);
                view->workspace = requested_output->current_workspace;
            }
        }
    }

    // If this view is actually a child of some parent, which is the
    // case for e.g. file dialogs, we should make it float instead of tiling
    if (guess_should_be_floating(view)) {
        view->is_floating = true;

        struct wlr_xdg_toplevel_state *current = &view->xdg_surface->toplevel->current;

        struct wlr_xdg_surface *xdg_surface = view->xdg_surface;
        struct wlr_box surface_geometry = { 0 };
        wlr_xdg_surface_get_geometry(xdg_surface, &surface_geometry);

        wlr_log(WLR_DEBUG,
                "Mapping xdg-shell surface \"%s\": actual width %d, actual height %d, width %d, height %d, "
                "min width %d, min height %d, max width %d, max height %d",
                view_name,
                surface_geometry.width,
                surface_geometry.height,
                current->width,
                current->height,
                current->min_width,
                current->min_height,
                current->max_width,
                current->max_height);


        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = surface_geometry.width;
        uint32_t height = surface_geometry.height;

        if (width < current->min_width) {
            width = current->min_width;
        }
        if (height < current->min_height) {
            height = current->min_height;
        }

        if (width < 2 && current->width >= 2) {
            width = current->width;
        }
        if (height < 2 && current->height >= 2) {
            height = current->height;
        }

        struct viv_output *output = view->workspace->output;
        if (output != NULL) {
            x += (uint32_t)(0.5 * output->wlr_output->width - 0.5 * width);
            y += (uint32_t)(0.5 * output->wlr_output->height - 0.5 * height);
        }
        width += view->server->config->border_width * 2;
        height += view->server->config->border_width * 2;
        viv_view_set_target_box(view, x, y, width, height);

    }
    if (view->server->config->allow_fullscreen &&
        !fullscreen_request_denied &&
        viv_view_set_fullscreen(view, requested->fullscreen)) {

        view->xdg_surface->toplevel->pending.fullscreen = requested->fullscreen;
        wlr_xdg_surface_schedule_configure(view->xdg_surface);
    }

    viv_workspace_add_view(view->workspace, view);

    view->surface_tree = viv_surface_tree_root_create(view->server, view->xdg_surface->surface, &add_xdg_view_global_coords, view);
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;

    viv_view_ensure_not_active_in_workspace(view);

    wl_list_remove(&view->workspace_link);
    wl_list_insert(&view->server->unmapped_views, &view->workspace_link);

    struct viv_workspace *workspace = view->workspace;
    viv_workspace_mark_for_relayout(workspace);

    viv_view_damage(view);

    viv_surface_tree_destroy(view->surface_tree);
    view->surface_tree = NULL;
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is destroyed and should never be shown again. */
	struct viv_view *view = wl_container_of(listener, view, destroy);
    wlr_log(WLR_INFO, "Destroying xdg-surface with view at %p", view);

    if (view->surface_tree) {
        viv_surface_tree_destroy(view->surface_tree);
        view->surface_tree = NULL;
    }

    viv_view_destroy(view);

    viv_server_clear_view_from_grab_state(view->server, view);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provied serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct viv_view *view = wl_container_of(listener, view, request_move);
    struct viv_seat *seat = viv_server_get_default_seat(view->server);
	viv_seat_begin_interactive(seat, view, VIV_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct viv_view *view = wl_container_of(listener, view, request_resize);
    struct viv_seat *seat = viv_server_get_default_seat(view->server);
	viv_seat_begin_interactive(seat, view, VIV_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, request_maximize);
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_ERROR, "\"%s\" requested maximise, ignoring", app_id);

    // We are required by the xdg-shell protocol to send a configure to acknowledge the
    // request, even if we ignored it
	wlr_xdg_surface_schedule_configure(view->xdg_surface);
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, request_minimize);
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_ERROR, "\"%s\" requested minimise, ignoring", app_id);
}

static void xdg_toplevel_set_title(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, set_title);
    const char *title = view->xdg_surface->toplevel->title;
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_DEBUG, "\"%s\" set title \"%s\"", app_id, title);
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	struct viv_view *view = wl_container_of(listener, view, request_fullscreen);
	struct wlr_xdg_toplevel_set_fullscreen_event *event = data;
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_DEBUG, "\"%s\" requested fullscreen %d", app_id, event->fullscreen);

    if (!view->server->config->allow_fullscreen) {
        wlr_log(WLR_DEBUG, "Ignoring \"%s\" fullscreen request. \"allow-fullscreen\" is set to false", app_id);
        wlr_xdg_surface_schedule_configure(event->surface);
        return;
    }

    if (event->surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        wlr_log(WLR_ERROR, "\"%s\" requested fullscreen %d for a non-toplevel surface", app_id, event->fullscreen);
        return;
    }

    bool fullscreen_request_denied = false;
    if (event->fullscreen && event->output) {
        struct viv_output *requested_output = viv_output_of_wlr_output(view->server, event->output);
        if (!requested_output) {
            wlr_log(WLR_ERROR, "Couldn't find requested fullscreen output for \"%s\"", app_id);
            fullscreen_request_denied = true;
        } else {
            if (requested_output->current_workspace->fullscreen_view) {
                wlr_log(WLR_DEBUG, "Output requested by \"%s\" already has a fullscreen view", app_id);
                fullscreen_request_denied = true;
            } else {
                wlr_log(WLR_DEBUG, "Moving \"%s\" to another workspace before attempting to go fullscreen", app_id);
                viv_view_shift_to_workspace(view, requested_output->current_workspace);
            }
        }
    }

    if (!fullscreen_request_denied && viv_view_set_fullscreen(view, event->fullscreen)) {
        event->surface->toplevel->pending.fullscreen = event->fullscreen;
    }
    wlr_xdg_surface_schedule_configure(event->surface);
}

static void implementation_set_size(struct viv_view *view, uint32_t width, uint32_t height) {
    ASSERT(view->type == VIV_VIEW_TYPE_XDG_SHELL);
    wlr_xdg_toplevel_set_size(view->xdg_surface, width, height);
}

static void implementation_set_pos(struct viv_view *view, uint32_t x, uint32_t y) {
    ASSERT(view->type == VIV_VIEW_TYPE_XDG_SHELL);

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

    view->x = x - geo_box.x;
    view->y = y - geo_box.y;
    // Nothing else to do, xdg surfaces don't know/care about their global pos
}

static void implementation_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    ASSERT(view->type == VIV_VIEW_TYPE_XDG_SHELL);
    wlr_xdg_surface_get_geometry(view->xdg_surface, geo_box);
    geo_box->x = view->x;
    geo_box->y = view->y;
}

static void implementation_set_tiled(struct viv_view *view, uint32_t edges) {
    ASSERT(view->type == VIV_VIEW_TYPE_XDG_SHELL);
    wlr_xdg_toplevel_set_tiled(view->xdg_surface, edges);
}

static void implementation_get_string_identifier(struct viv_view *view, char *output, size_t max_len) {
    snprintf(output, max_len, "<XDG:%s:%s>",
             view->xdg_surface->toplevel->app_id,
             view->xdg_surface->toplevel->title);
}

static void implementation_set_activated(struct viv_view *view, bool activated) {
	wlr_xdg_toplevel_set_activated(view->xdg_surface, activated);
}

static struct wlr_surface *implementation_get_toplevel_surface(struct viv_view *view) {
    return view->xdg_surface->surface;
}

static void implementation_close(struct viv_view *view) {
    wlr_xdg_toplevel_send_close(view->xdg_surface);
}

static bool implementation_is_at(struct viv_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;

	double _sx, _sy, _non_popup_sx, _non_popup_sy;

    // If the view is oversized, we don't let it draw outside its target region so can
    // stop immediately if the cursor is outside that region

    // Get surface at point from the full selection of popups and toplevel surfaces
	struct wlr_surface *_surface = NULL;
	_surface = wlr_xdg_surface_surface_at(
			view->xdg_surface, view_sx, view_sy, &_sx, &_sy);

    // Get surface at point considering only the non-popup surfaces
    struct wlr_surface *_non_popup_surface = NULL;
    _non_popup_surface = wlr_surface_surface_at(
        view->xdg_surface->surface, view_sx, view_sx, &_non_popup_sx, &_non_popup_sy);

    // We can only click on a toplevel surface if it's within the target render box,
    // otherwise that part isn't being drawn and shouldn't be accessible
    bool surface_is_popup = (_surface != _non_popup_surface);
    bool cursor_in_target_box = wlr_box_contains_point(&view->target_box, lx, ly);
    bool surface_clickable = (surface_is_popup || cursor_in_target_box);

	if ((_surface != NULL) && surface_clickable) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}

	return false;
}

static bool implementation_oversized(struct viv_view *view) {
    struct wlr_box actual_geometry = { 0 };
    struct wlr_box *target_geometry = &view->target_box;
    wlr_xdg_surface_get_geometry(view->xdg_surface, &actual_geometry);

    int leeway_px = 00;

    bool surface_exceeds_bounds = ((actual_geometry.width > (target_geometry->width + leeway_px)) ||
                                   (actual_geometry.height > (target_geometry->height + leeway_px)));

    return surface_exceeds_bounds;
}

static void implementation_inform_unrequested_fullscreen_change(struct viv_view *view) {
    view->xdg_surface->toplevel->pending.fullscreen = (view->workspace->fullscreen_view == view);
    wlr_xdg_surface_schedule_configure(view->xdg_surface);
}

static void implementation_grow_and_center_fullscreen(struct viv_view *view) {
    struct wlr_xdg_toplevel_state *current = &view->xdg_surface->toplevel->current;

    struct viv_output *output = view->workspace->output;
    if (!output) {
        return;
    }

    int width = output->wlr_output->width;
    if (current->max_width && ((int) current->max_width < width)) {
        width = current->max_width;
    } else if (current->min_width && ((int) current->min_width > width)) {
        width = current->min_width;
    }

    int height = output->wlr_output->height;
    if (current->max_height && ((int) current->max_height < height)) {
        height = current->max_height;
    } else if (current->min_height && ((int) current->min_height > height)) {
        height = current->min_height;
    }

    int x = (output->wlr_output->width - width) / 2;
    int y = (output->wlr_output->height - height) / 2;
    viv_view_set_target_box(view, x, y, width, height);
}

static struct viv_view_implementation xdg_view_implementation = {
    .set_size = &implementation_set_size,
    .set_pos = &implementation_set_pos,
    .get_geometry = &implementation_get_geometry,
    .set_tiled = &implementation_set_tiled,
    .get_string_identifier = &implementation_get_string_identifier,
    .set_activated = &implementation_set_activated,
    .get_toplevel_surface = &implementation_get_toplevel_surface,
    .close = &implementation_close,
    .is_at = &implementation_is_at,
    .oversized = &implementation_oversized,
    .inform_unrequested_fullscreen_change = &implementation_inform_unrequested_fullscreen_change,
    .grow_and_center_fullscreen = &implementation_grow_and_center_fullscreen,
};

static void handle_xdg_surface_new_popup(struct wl_listener *listener, void *data) {
    struct viv_view *view = wl_container_of(listener, view, new_xdg_popup);
	struct wlr_xdg_popup *wlr_popup = data;

    struct viv_xdg_popup *popup = calloc(1, sizeof(struct viv_xdg_popup));
    popup->server = view->server;
    popup->lx = &view->x;
    popup->ly = &view->y;
    viv_xdg_popup_init(popup, wlr_popup);
}

void viv_xdg_view_init(struct viv_view *view, struct wlr_xdg_surface *xdg_surface) {

    view->type = VIV_VIEW_TYPE_XDG_SHELL;
    view->implementation = &xdg_view_implementation;
	view->xdg_surface = xdg_surface;

	/* Listen to the various events it can emit */
	view->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

    view->new_xdg_popup.notify = handle_xdg_surface_new_popup;
    wl_signal_add(&xdg_surface->events.new_popup, &view->new_xdg_popup);

	/* cotd */
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	view->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
	view->set_title.notify = xdg_toplevel_set_title;
	wl_signal_add(&toplevel->events.set_title, &view->set_title);
	view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);

}
