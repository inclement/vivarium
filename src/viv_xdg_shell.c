#include <stdlib.h>
#include <wlr/util/log.h>

#include "viv_config_support.h"
#include "viv_xdg_shell.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"

/// Return true if the view looks like it should be floating.
// TODO: Make this more robust
static bool guess_should_be_floating(struct viv_view *view) {
    return (view->xdg_surface->toplevel->parent != NULL);
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;

    wl_list_remove(&view->workspace_link);

    // If this view is actually a child of some parent, which is the
    // case for e.g. file dialogs, we should make it float instead of tiling
    if (guess_should_be_floating(view)) {
        view->is_floating = true;

        // TODO: we shouldn't use the minimum size, probably actually
        // want some indication of fractional pos and size relative to
        // workspace output, which is handled during layout
        struct wlr_xdg_toplevel_state *current = &view->xdg_surface->toplevel->current;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = current->min_width;
        uint32_t height = current->min_height;
        struct viv_output *output = view->workspace->output;
        if (output != NULL) {
            x += (uint32_t)(0.5 * output->wlr_output->width - 0.5 * width);
            y += (uint32_t)(0.5 * output->wlr_output->height - 0.5 * height);
        }
        width += view->server->config->border_width * 2;
        height += view->server->config->border_width * 2;
        viv_view_set_target_box(view, x, y, width, height);

    }

    viv_workspace_add_view(view->workspace, view);
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
    workspace->needs_layout = true;
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is destroyed and should never be shown again. */
	struct viv_view *view = wl_container_of(listener, view, destroy);
    viv_view_destroy(view);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provied serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct viv_view *view = wl_container_of(listener, view, request_move);
	viv_server_begin_interactive(view, VIV_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct viv_view *view = wl_container_of(listener, view, request_resize);
	viv_server_begin_interactive(view, VIV_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, request_maximize);
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_ERROR, "\"%s\" requested maximise, ignoring", app_id);
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
    struct wlr_box target_box = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height,
    };

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
    bool cursor_in_target_box = wlr_box_contains_point(&target_box, lx, ly);
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
    struct wlr_box target_geometry = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height
    };
    wlr_xdg_surface_get_geometry(view->xdg_surface, &actual_geometry);

    int leeway_px = 00;

    bool surface_exceeds_bounds = ((actual_geometry.width > (target_geometry.width + leeway_px)) ||
                                   (actual_geometry.height > (target_geometry.height + leeway_px)));

    return surface_exceeds_bounds;
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
};

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

}
