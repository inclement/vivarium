
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"
#include "viv_xwayland_shell.h"

static void event_xwayland_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	viv_view_focus(view, view->xdg_surface->surface);

    struct viv_workspace *workspace = view->workspace;
    workspace->needs_layout = true;
}

static void event_xwayland_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;

    struct viv_workspace *workspace = view->workspace;
    workspace->needs_layout = true;
}

static void event_xwayland_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, destroy);
    viv_view_destroy(view);
}

static void implementation_set_size(struct viv_view *view, uint32_t width, uint32_t height) {
    ASSERT(false);
    UNUSED(view);
    UNUSED(width);
    UNUSED(height);
}

static void implementation_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    ASSERT(false);
    UNUSED(view);
    UNUSED(geo_box);
}

static struct viv_view_implementation xwayland_view_implementation = {
    .set_size = &implementation_set_size,
    .get_geometry = &implementation_get_geometry,
};

void viv_xwayland_view_init(struct viv_view *view, struct wlr_xwayland_surface *xwayland_surface) {
    view->type = VIV_VIEW_TYPE_XDG_SHELL;
    view->implementation = &xwayland_view_implementation;
	view->xwayland_surface = xwayland_surface;

	view->map.notify =event_xwayland_surface_map;
	wl_signal_add(&xwayland_surface->events.map, &view->map);
	view->unmap.notify = event_xwayland_surface_unmap;
	wl_signal_add(&xwayland_surface->events.unmap, &view->unmap);
	view->destroy.notify = event_xwayland_surface_destroy;
	wl_signal_add(&xwayland_surface->events.destroy, &view->destroy);
}
