#include <pixman-1/pixman.h>
#include <wlr/types/wlr_output_damage.h>
#include <xcb/xcb.h>

#include "viv_damage.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_wlr_surface_tree.h"
#include "viv_workspace.h"
#include "viv_xwayland_shell.h"
#include "viv_xwayland_types.h"


#define AS_STR(ATOM) #ATOM,

static const char *window_type_atom_strings[] = {
    MACRO_FOR_EACH_ATOM_NAME(AS_STR)
};

#ifdef DEBUG
static void log_window_type_strings(struct wlr_xwayland_surface *surface) {
    xcb_connection_t *xcb_connection = xcb_connect(NULL, NULL);
    int error = xcb_connection_has_error(xcb_connection);
    if (error) {
        wlr_log(WLR_ERROR, "Could not connect to XCB, error %d", error);
        // No point actually crashing, this will just mean we can't identify window types later
        return;
    }

    // This is standard xcb interaction: get cookies for each request then get the
    // replies, to let xcb assemble the results asynchronously
    xcb_get_atom_name_cookie_t cookies[WINDOW_TYPE_ATOM_MAX];
    for (size_t i = 0; i < surface->window_type_len; i++) {
        cookies[i] = xcb_get_atom_name(xcb_connection, surface->window_type[i]);
    }

    for (size_t i = 0; i < surface->window_type_len; i++) {
        xcb_generic_error_t *error = NULL;
        xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply(xcb_connection, cookies[i], &error);
        if (reply != NULL && error == NULL) {
            wlr_log(WLR_INFO, "New X11 window with title \"%s\" has window type %s",
                    surface->title, xcb_get_atom_name_name(reply));
        }
        free(reply);

        if (error != NULL) {
            wlr_log(WLR_ERROR, "Failed to lookup atom %d, X11 error code %d",
                    surface->window_type[i], error->error_code);
            free(error);
            break;
        }
    }

	xcb_disconnect(xcb_connection);
}
#endif

/// Return true if the view looks like it should be floating according to its window type atoms.
static bool guess_should_be_floating(struct viv_view *view) {

    struct wlr_xwayland_surface *surface = view->xwayland_surface;
    struct wlr_xwayland_surface_size_hints *size_hints = view->xwayland_surface->size_hints;

    xcb_atom_t *window_type_atoms = view->server->window_type_atoms;

    // If the window is marked as a type that is fundamentally floating, obey that
    for (size_t i = 0; i < surface->window_type_len; i++) {
        xcb_atom_t window_type = surface->window_type[i];
        if ((window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_DIALOG]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_UTILITY]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_TOOLBAR]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_POPUP_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_DROPDOWN_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_TOOLTIP]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_COMBO]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_SPLASH])) {
            return true;
        }
    }

    // Guess whether the window wants to be floating based on its parent (if any) and size hints
    return (size_hints &&
            (view->xwayland_surface->parent != NULL ||
             ((size_hints->min_width == size_hints->max_width) && (size_hints->min_height == size_hints->max_height))));
}

/// Use the X11 window properties to work out if it's a view to be managed or something like a dialog
static bool guess_should_have_no_borders(struct viv_view *view) {
    struct wlr_xwayland_surface *surface = view->xwayland_surface;

    xcb_atom_t *window_type_atoms = view->server->window_type_atoms;

    // If the view is marked as a type that doesn't merit borders (i.e. dialog etc.), obey that
    for (size_t i = 0; i < surface->window_type_len; i++) {
        xcb_atom_t window_type = surface->window_type[i];
        if ((window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_POPUP_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_DROPDOWN_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_MENU]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_TOOLTIP]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_COMBO]) ||
            (window_type == window_type_atoms[_NET_WM_WINDOW_TYPE_SPLASH])) {
            return true;
        }
    }

    // If the view has a parent, assume this means it's a right click menu or something.
    // TODO: This might be wrong - if it is, probably the correct solution is to rely
    // solely on window type instead
    return view->xwayland_surface->parent != NULL;
}

static void add_xwayland_view_global_coords(void *view_pointer, int *x, int *y) {
    struct viv_view *view = view_pointer;
    *x += view->x;
    *y += view->y;
}

static void event_xwayland_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
    wlr_log(WLR_DEBUG, "xwayland surface mapped");
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;

    uint32_t view_name_len = 200;
    char view_name[view_name_len];
    viv_view_get_string_identifier(view, view_name, view_name_len);
    struct wlr_xwayland_surface *surface = view->xwayland_surface;
    struct wlr_xwayland_surface_size_hints *size_hints = surface->size_hints;
    if (size_hints) {
        wlr_log(WLR_DEBUG,
                "Mapping xwayland surface \"%s\": actual width %d, actual height %d, width %d, height %d, "
                "min width %d, min height %d, max width %d, max height %d, base width %d, base height %d, fullscreen %d, parent %p",
                view_name,
                surface->width,
                surface->height,
                size_hints->width,
                size_hints->height,
                size_hints->min_height,
                size_hints->min_width,
                size_hints->max_height,
                size_hints->max_width,
                size_hints->base_height,
                size_hints->base_width,
                surface->fullscreen,
                surface->parent);
    }

#ifdef DEBUG
    log_window_type_strings(surface);
#endif

    if (guess_should_be_floating(view)) {
        view->is_floating = true;

        if (guess_should_have_no_borders(view)) {
            view->is_static = true;
        }

        uint32_t x = surface->x;
        uint32_t y = surface->y;
        uint32_t width = 500;
        uint32_t height = 300;

        if (surface->width && surface->height) {
            width = surface->width;
            height = surface->height;
        }

        if (size_hints) {
            // `width` and `height` are deprecated. base_{width,height} indicate the desired size
            if (size_hints->base_width > 0 && size_hints->base_height > 0) {
                width = size_hints->base_width;
                height = size_hints->base_height;
            }
            if (size_hints->min_width > 0 && width < (uint32_t) size_hints->min_width) {
                width = size_hints->min_width;
            }
            if (size_hints->min_height > 0 && height < (uint32_t) size_hints->min_height) {
                height = size_hints->min_height;
            }
            if (size_hints->max_width > 0 && width > (uint32_t) size_hints->max_width) {
                width = size_hints->max_width;
            }
            if (size_hints->max_height > 0 && height > (uint32_t) size_hints->max_height) {
                height = size_hints->max_height;
            }
        }

        if (x == 0 && y == 0) {
            // Guess that this is a new popup window that should be centered.  This obviously does
            // the wrong thing in the case of a transient popup having 0,0 as the true position, but
            // it's better than nothing for now.
            // TODO: Make this nice by actually checking the window type
            struct viv_output *output = view->workspace->output;
            if (output != NULL) {
                x += (uint32_t)(0.5 * output->wlr_output->width - 0.5 * width);
                y += (uint32_t)(0.5 * output->wlr_output->height - 0.5 * height);
            }
        }
        width += view->server->config->border_width * 2;
        height += view->server->config->border_width * 2;

        viv_view_set_target_box(view, x, y, width, height);
    }


    wl_list_remove(&view->workspace_link);

    viv_workspace_add_view(view->workspace, view);

    if (view->server->config->allow_fullscreen) {
        viv_view_set_fullscreen(view, surface->fullscreen);
        wlr_xwayland_surface_set_fullscreen(surface, view->workspace->fullscreen_view == view);
    }

    view->surface_tree = viv_surface_tree_root_create(view->server, view->xwayland_surface->surface, &add_xwayland_view_global_coords, view);
}

static void event_xwayland_surface_unmap(struct wl_listener *listener, void *data) {
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

    viv_server_clear_view_from_grab_state(view->server, view);
}

static void event_xwayland_request_fullscreen(struct wl_listener *listener, void *data) {
    struct viv_view *view = wl_container_of(listener, view, request_fullscreen);
    struct wlr_xwayland_surface *surface = data;

    const char *class = view->xwayland_surface->class;
    wlr_log(WLR_DEBUG, "\"%s\" requested fullscreen %d", class, surface->fullscreen);

    if (!view->server->config->allow_fullscreen) {
        wlr_log(WLR_DEBUG, "Ignoring \"%s\" fullscreen request. \"allow-fullscreen\" is set to false", class);
        return;
    }

    // Sometimes fullscreen is requested before mapping. Explicitly handling the request
    // here would mean the fullscreen attribute would be gone when mapping
    if (!view->mapped) {
        wlr_log(WLR_DEBUG, "\"%s\" fullscreen request ignored as it hasn't been mapped yet", class);
        return;
    }

    viv_view_set_fullscreen(view, surface->fullscreen);
    wlr_xwayland_surface_set_fullscreen(surface, view->workspace->fullscreen_view == view );
}

static void event_xwayland_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, destroy);
    viv_view_destroy(view);
}

static void implementation_set_size(struct viv_view *view, uint32_t width, uint32_t height) {
    wlr_xwayland_surface_configure(view->xwayland_surface, view->x, view->y, width, height);
}

static void implementation_set_pos(struct viv_view *view, uint32_t x, uint32_t y) {
    view->x = x;
    view->y = y;
    wlr_xwayland_surface_configure(view->xwayland_surface, x, y, view->target_box.width, view->target_box.height);
}

static void implementation_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    geo_box->x = view->x;
    geo_box->y = view->y;
    geo_box->width = view->xwayland_surface->width;
    geo_box->height = view->xwayland_surface->height;
}

static void implementation_set_tiled(struct viv_view *view, uint32_t edges) {
    ASSERT(view->type == VIV_VIEW_TYPE_XWAYLAND);
    UNUSED(edges);
    wlr_log(WLR_DEBUG, "Tried to set tiled attribute of xwayland window to %d, ignoring", edges);
}

static void implementation_get_string_identifier(struct viv_view *view, char *output, size_t max_len) {
    snprintf(output, max_len, "<XWAY:%s:%s>",
             view->xwayland_surface->title,
             view->xwayland_surface->class);
}

static void implementation_set_activated(struct viv_view *view, bool activated) {
	wlr_xwayland_surface_activate(view->xwayland_surface, activated);
    if (activated) {
        wlr_xwayland_surface_restack(view->xwayland_surface, NULL, XCB_STACK_MODE_ABOVE);
    }
}

static struct wlr_surface *implementation_get_toplevel_surface(struct viv_view *view) {
    return view->xwayland_surface->surface;
}

static void implementation_close(struct viv_view *view) {
    wlr_xwayland_surface_close(view->xwayland_surface);
}

static bool implementation_is_at(struct viv_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;

	double _sx, _sy;

    // If the view is oversized, we don't let it draw outside its target region so can
    // stop immediately if the cursor is outside that region

    // Get surface at point from the full selection of popups and toplevel surfaces
	struct wlr_surface *_surface = NULL;
	_surface = wlr_surface_surface_at(view->xwayland_surface->surface, view_sx, view_sy, &_sx, &_sy);

    // We can only click on a toplevel surface if it's within the target render box,
    // otherwise that part isn't being drawn and shouldn't be accessible
    bool cursor_in_target_box = wlr_box_contains_point(&view->target_box, lx, ly);

	if ((_surface != NULL) && cursor_in_target_box) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}

	return false;
}

static bool implementation_oversized(struct viv_view *view) {
    struct wlr_box actual_geometry = {
        .width = view->xwayland_surface->surface->current.width,
        .height = view->xwayland_surface->surface->current.height,
    };
    struct wlr_box *target_geometry = &view->target_box;

    bool surface_exceeds_bounds = ((actual_geometry.width > (target_geometry->width)) ||
                                   (actual_geometry.height > (target_geometry->height)));

    return surface_exceeds_bounds;
}

static void implementation_inform_unrequested_fullscreen_change(struct viv_view *view) {
    wlr_xwayland_surface_set_fullscreen(view->xwayland_surface, view->workspace->fullscreen_view == view);
}

static void implementation_grow_and_center_fullscreen(struct viv_view *view) {
    struct wlr_xwayland_surface_size_hints *hints = view->xwayland_surface->size_hints;

    struct viv_output *output = view->workspace->output;
    if (!output) {
        return;
    }

    int width = output->wlr_output->width;
    if ((hints->max_width > 0) && ((int) hints->max_width < width)) {
        width = hints->max_width;
    } else if ((hints->min_width > 0) && ((int) hints->min_width > width)) {
        width = hints->min_width;
    }

    int height = output->wlr_output->height;
    if ((hints->max_height > 0) && ((int) hints->max_height < height)) {
        height = hints->max_height;
    } else if ((hints->min_height > 0) && ((int) hints->min_height > height)) {
        height = hints->min_height;
    }

    int x = (output->wlr_output->width - width) / 2;
    int y = (output->wlr_output->height - height) / 2;
    viv_view_set_target_box(view, x, y, width, height);
}

static struct viv_view_implementation xwayland_view_implementation = {
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

void viv_xwayland_view_init(struct viv_view *view, struct wlr_xwayland_surface *xwayland_surface) {
    view->type = VIV_VIEW_TYPE_XWAYLAND;
    view->implementation = &xwayland_view_implementation;
	view->xwayland_surface = xwayland_surface;

	view->map.notify =event_xwayland_surface_map;
	wl_signal_add(&xwayland_surface->events.map, &view->map);
	view->unmap.notify = event_xwayland_surface_unmap;
	wl_signal_add(&xwayland_surface->events.unmap, &view->unmap);
	view->destroy.notify = event_xwayland_surface_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);
    view->request_fullscreen.notify = event_xwayland_request_fullscreen;
    wl_signal_add(&xwayland_surface->events.request_fullscreen, &view->request_fullscreen);
}

void viv_xwayland_lookup_atoms(struct viv_server *server) {
    xcb_connection_t *xcb_connection = xcb_connect(NULL, NULL);
    int error = xcb_connection_has_error(xcb_connection);
    if (error) {
        wlr_log(WLR_ERROR, "Could not connect to XCB, error %d", error);
        // No point actually crashing, this will just mean we can't identify window types later
        return;
    }

    // This is standard xcb interaction: get cookies for each request then get the
    // replies, to let xcb assemble the results asynchronously
    xcb_intern_atom_cookie_t cookies[WINDOW_TYPE_ATOM_MAX];
    for (size_t i = 0; i < WINDOW_TYPE_ATOM_MAX; i++) {
        const char *atom_str = window_type_atom_strings[i];
        cookies[i] = xcb_intern_atom(xcb_connection, false, strlen(atom_str), atom_str);
    }

    for (size_t i = 0; i < WINDOW_TYPE_ATOM_MAX; i++) {
        xcb_generic_error_t *error = NULL;
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(xcb_connection, cookies[i], &error);
        if (reply != NULL && error == NULL) {
            server->window_type_atoms[i] = reply->atom;
        }
        free(reply);

        if (error != NULL) {
            wlr_log(WLR_ERROR, "Failed to lookup atom name %s, X11 error code %d",
                    window_type_atom_strings[i], error->error_code);
            free(error);
            break;
        }
    }
	xcb_disconnect(xcb_connection);
}
