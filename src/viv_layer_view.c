#include <wayland-util.h>

#include "viv_layer_view.h"
#include "viv_output.h"
#include "viv_types.h"

static void layer_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
    wlr_log(WLR_DEBUG, "Mapping a layer surface");
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, map);
	layer_view->mapped = true;
}

static void layer_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, unmap);
	layer_view->mapped = false;
}

static void layer_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is destroyed and should never be shown again. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, destroy);

    // TODO: unfocus this as the keyboard surface if necessary

	wl_list_remove(&layer_view->output_link);

	free(layer_view);
}

static void layer_surface_new_popup(struct wl_listener *listener, void *data) {
    UNUSED(listener);
    UNUSED(data);
    wlr_log(WLR_ERROR, "New layer surface popup event not yet handled");
}

void viv_layer_view_init(struct viv_layer_view *layer_view, struct viv_server *server, struct wlr_layer_surface_v1 *layer_surface) {
    layer_view->layer_surface = layer_surface;
    layer_view->server = server;
    layer_view->mapped = false;

	layer_view->map.notify = layer_surface_map;
	wl_signal_add(&layer_surface->events.map, &layer_view->map);
	layer_view->unmap.notify = layer_surface_unmap;
	wl_signal_add(&layer_surface->events.unmap, &layer_view->unmap);
	layer_view->destroy.notify = layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy, &layer_view->destroy);
	/* layer_view->new_popup.notify = layer_surface_new_popup; */
	/* wl_signal_add(&layer_surface->events.new_popup, &layer_view->new_popup); */
    UNUSED(layer_surface_new_popup);

    struct viv_output *output = viv_output_of_wlr_output(server, layer_surface->output);
    wl_list_insert(&output->layer_views, &layer_view->output_link);
    layer_view->output = output;
}
