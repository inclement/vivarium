
#include "viv_output.h"
#include "viv_types.h"

#include "viv_xdg_popup.h"

/// Add to x and y the global (i.e. output-layout) coords of the input popup, calculated
/// by walking up the popup tree and adding the geometry of each parent.
static void add_popup_global_coords(void *popup_pointer, int *x, int *y) {
    struct viv_xdg_popup *popup = popup_pointer;

    int px = 0;
    int py = 0;

    struct viv_xdg_popup *cur_popup = popup;
    while (true) {
        px += cur_popup->wlr_popup->current.geometry.x;
        py += cur_popup->wlr_popup->current.geometry.y;

        if (cur_popup->parent_popup != NULL) {
            cur_popup = cur_popup->parent_popup;
        } else {
            break;
        }
    }

    px += *popup->lx;
    py += *popup->ly;

    *x += px;
    *y += py;
}

static void handle_popup_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, surface_map);
    wlr_log(WLR_INFO, "Map popup at %p", popup);
}

static void handle_popup_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, surface_unmap);

    wlr_log(WLR_INFO, "Unmap popup at %p", popup);

    int px = 0;
    int py = 0;
    add_popup_global_coords(popup, &px, &py);
}

static void handle_popup_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, destroy);
    wlr_log(WLR_INFO, "Popup at %p being destroyed", popup);
    free(popup);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
    struct viv_xdg_popup *popup = wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

    struct viv_xdg_popup *new_popup = calloc(1, sizeof(struct viv_xdg_popup));
    new_popup->server = popup->server;
    new_popup->lx = popup->lx;
    new_popup->ly = popup->ly;
    new_popup->parent_popup = popup;
    viv_xdg_popup_init(new_popup, wlr_popup);
}

/// Set the region in which the popup can be displayed, so that its position is shifted to
/// stay within its output and not be rendered offscreen
static void popup_unconstrain(struct viv_xdg_popup *popup) {
    struct viv_output *output = popup->server->active_output;
    if (!output) {
        wlr_log(WLR_ERROR, "Cannot unconstraint popup, no active output");
    }

    struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;

    double lx = 0, ly = 0;
    wlr_output_layout_output_coords(output->server->output_layout, output->wlr_output, &lx, &ly);

    if (!output) {
        wlr_log(WLR_ERROR, "Tried to unconstrain a popup that doesn't have an output");
        return;
    }

    // The output box is relative to the popup's toplevel parent
    struct wlr_box output_box = {
        .x = lx - *popup->lx,
        .y = ly - *popup->ly,
        .width = output->wlr_output->width,
        .height = output->wlr_output->height,
    };

    wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_box);
}

void viv_xdg_popup_init(struct viv_xdg_popup *popup, struct wlr_xdg_popup *wlr_popup) {
    popup->wlr_popup = wlr_popup;

    wlr_log(WLR_INFO, "New popup %p with parent %p", popup, popup->parent_popup);


    popup->surface_map.notify = handle_popup_surface_map;
    wl_signal_add(&wlr_popup->base->events.map, &popup->surface_map);
    popup->surface_unmap.notify = handle_popup_surface_unmap;
    wl_signal_add(&wlr_popup->base->events.unmap, &popup->surface_unmap);

    popup->destroy.notify = handle_popup_surface_destroy;
    wl_signal_add(&wlr_popup->base->surface->events.destroy, &popup->destroy);

    popup->new_popup.notify = handle_new_popup;
    wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);

    popup_unconstrain(popup);

    wlr_log(WLR_INFO, "New wlr_popup surface at %p", wlr_popup->base->surface);
}
