#include <stdlib.h>
#include <wlr/util/log.h>

#include "viv_config_support.h"
#include "viv_xdg_shell.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"

static void xdg_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	viv_view_focus(view, viv_view_get_toplevel_surface(view));

    struct viv_workspace *workspace = view->workspace;
    workspace->output->needs_layout = true;
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;

    struct viv_workspace *workspace = view->workspace;
    workspace->output->needs_layout = true;
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

static struct viv_view_implementation xdg_view_implementation = {
    .set_size = &implementation_set_size,
    .get_geometry = &implementation_get_geometry,
    .set_tiled = &implementation_set_tiled,
    .get_string_identifier = &implementation_get_string_identifier,
    .set_activated = &implementation_set_activated,
    .get_toplevel_surface = &implementation_get_toplevel_surface,
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
