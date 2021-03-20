
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"
#include "viv_xwayland_shell.h"

/* /// See documentation at https://specifications.freedesktop.org/wm-spec/1.4/ar01s05.html */
/* static const char *window_type_atom_strings[] = { */
/*     "_NET_WM_WINDOW_TYPE_DESKTOP", */
/*     "_NET_WM_WINDOW_TYPE_DOCK", */
/*     "_NET_WM_WINDOW_TYPE_TOOLBAR", */
/*     "_NET_WM_WINDOW_TYPE_MENU", */
/*     "_NET_WM_WINDOW_TYPE_UTILITY", */
/*     "_NET_WM_WINDOW_TYPE_SPLASH", */
/*     "_NET_WM_WINDOW_TYPE_DIALOG", */
/*     "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", */
/*     "_NET_WM_WINDOW_TYPE_POPUP_MENU", */
/*     "_NET_WM_WINDOW_TYPE_TOOLTIP", */
/*     "_NET_WM_WINDOW_TYPE_NOTIFICATION", */
/*     "_NET_WM_WINDOW_TYPE_COMBO", */
/*     "_NET_WM_WINDOW_TYPE_DND", */
/*     "_NET_WM_WINDOW_TYPE_NORMAL", */
/* }; */

/* enum window_type_atom { */
/*     NET_WM_WINDOW_TYPE_DESKTOP, */
/*     NET_WM_WINDOW_TYPE_DOCK, */
/*     NET_WM_WINDOW_TYPE_TOOLBAR, */
/*     NET_WM_WINDOW_TYPE_MENU, */
/*     NET_WM_WINDOW_TYPE_UTILITY, */
/*     NET_WM_WINDOW_TYPE_SPLASH, */
/*     NET_WM_WINDOW_TYPE_DIALOG, */
/*     NET_WM_WINDOW_TYPE_DROPDOWN_MENU, */
/*     NET_WM_WINDOW_TYPE_POPUP_MENU, */
/*     NET_WM_WINDOW_TYPE_TOOLTIP, */
/*     NET_WM_WINDOW_TYPE_NOTIFICATION, */
/*     NET_WM_WINDOW_TYPE_COMBO, */
/*     NET_WM_WINDOW_TYPE_DND, */
/*     NET_WM_WINDOW_TYPE_NORMAL, */
/*     WINDOW_TYPE_ATOM_MAX, */
/* }; */

/* static uint32_t num_atoms = sizeof(window_type_atom_strings) / sizeof(char *); */


/// Return true if the view looks like it should be floating. This is a poor proxy for proper window type inspection.
/// TODO: Do proper window type inspection.
static bool guess_should_be_floating(struct viv_view *view) {

    struct wlr_xwayland_surface_size_hints *size_hints = view->xwayland_surface->size_hints;

    // TODO: Use the window_type to guess this, not just the size hints

    return (size_hints &&
            (view->xwayland_surface->parent != NULL ||
             ((size_hints->min_width == size_hints->max_width) && (size_hints->min_height == size_hints->max_height))));
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
                "min width %d, min height %d, max width %d, max height %d, base width %d, base height %d, parent %p",
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
                surface->parent);
    }

    if (guess_should_be_floating(view)) {
        // Assume this is a popup or something, and just give it the size it wants
        // TODO: There is probably a better/proper way to do this based on actual window type parsing
        view->is_floating = true;
        view->is_static = true;

        uint32_t x = surface->x;
        uint32_t y = surface->y;
        uint32_t width = surface->width;
        uint32_t height = surface->height;

        if (size_hints->min_width >= 0 && size_hints->min_height >= 0) {
            width = size_hints->min_width;
            height = size_hints->min_height;
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
}

static void event_xwayland_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;


	struct wlr_seat *seat = view->workspace->server->seat;
    seat->keyboard_state.focused_surface = NULL;
    viv_view_ensure_not_active_in_workspace(view);

    wl_list_remove(&view->workspace_link);
    wl_list_insert(&view->server->unmapped_views, &view->workspace_link);

    struct viv_workspace *workspace = view->workspace;
    workspace->needs_layout = true;
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
    wlr_xwayland_surface_configure(view->xwayland_surface, x, y, view->target_width, view->target_height);
}

static void implementation_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    geo_box->x = 0;
    geo_box->y = 0;
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
    struct wlr_box target_box = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height,
    };

    // Get surface at point from the full selection of popups and toplevel surfaces
	struct wlr_surface *_surface = NULL;
	_surface = wlr_surface_surface_at(view->xwayland_surface->surface, view_sx, view_sy, &_sx, &_sy);

    // We can only click on a toplevel surface if it's within the target render box,
    // otherwise that part isn't being drawn and shouldn't be accessible
    bool cursor_in_target_box = wlr_box_contains_point(&target_box, lx, ly);

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
    struct wlr_box target_geometry = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height
    };

    bool surface_exceeds_bounds = ((actual_geometry.width > (target_geometry.width)) ||
                                   (actual_geometry.height > (target_geometry.height)));

    return surface_exceeds_bounds;
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
}
