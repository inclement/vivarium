#include <pixman-1/pixman.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_damage.h>

#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "viv_cursor.h"
#include "viv_damage.h"
#include "viv_layer_view.h"
#include "viv_output.h"
#include "viv_seat.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_wlr_surface_tree.h"
#include "viv_xdg_popup.h"

static void add_layer_view_global_coords(void *layer_view_pointer, int *x, int *y) {
    struct viv_layer_view *view = layer_view_pointer;
    *x += view->x;
    *y += view->y;
}

static void layer_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
    wlr_log(WLR_DEBUG, "Mapping a layer surface");
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, map);

    viv_output_mark_for_relayout(layer_view->output);

    layer_view->surface_tree = viv_surface_tree_root_create(layer_view->server, layer_view->layer_surface->surface, &add_layer_view_global_coords, layer_view);

    if (layer_view->layer_surface->current.keyboard_interactive) {
        struct viv_seat *seat = viv_server_get_default_seat(layer_view->server);
        viv_seat_focus_surface(seat, layer_view->layer_surface->surface);
    }
}

static void layer_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, unmap);
	layer_view->mapped = false;

    viv_output_mark_for_relayout(layer_view->output);

	struct viv_seat *seat = viv_server_get_default_seat(layer_view->server);
	struct wlr_surface *prev_surface = seat->wlr_seat->keyboard_state.focused_surface;
    struct wlr_surface *our_surface = layer_view->layer_surface->surface;
    if (prev_surface == our_surface) {
        seat->wlr_seat->keyboard_state.focused_surface = NULL;
    }

    struct viv_view *active_view = NULL;
    if (layer_view->server->active_output) {
        active_view = layer_view->server->active_output->current_workspace->active_view;
    }

    if (active_view != NULL) {
        viv_view_focus(active_view, NULL);
    }

    if (layer_view->surface_tree) {
        viv_surface_tree_destroy(layer_view->surface_tree);
        layer_view->surface_tree = NULL;
    } else {
        wlr_log(WLR_ERROR, "Layer view had no surface tree during unmap");
    }
}

static void layer_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is destroyed and should never be shown again. */
	struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, destroy);

    // TODO: unfocus this as the keyboard surface if necessary

	wl_list_remove(&layer_view->output_link);

    viv_output_mark_for_relayout(layer_view->output);

    if (layer_view->surface_tree) {
        viv_surface_tree_destroy(layer_view->surface_tree);
        layer_view->surface_tree = NULL;
    }

    wl_list_remove(&layer_view->map.link);
    wl_list_remove(&layer_view->unmap.link);
    wl_list_remove(&layer_view->destroy.link);
    wl_list_remove(&layer_view->new_popup.link);

	free(layer_view);
}


static void layer_surface_new_popup(struct wl_listener *listener, void *data) {
    struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

    struct viv_xdg_popup *popup = calloc(1, sizeof(struct viv_xdg_popup));

    popup->lx = &layer_view->x;
    popup->ly = &layer_view->y;
    popup->server = layer_view->server;
    viv_xdg_popup_init(popup, wlr_popup);

}

static void layer_surface_surface_commit(struct wl_listener *listener, void *data) {
    struct viv_layer_view *layer_view = wl_container_of(listener, layer_view, surface_commit);
    UNUSED(data);

    if (!layer_view->layer_surface.current.committed && layer_view->layer_surface.mapped == layer_view->mapped) {
        return;
    }

    layer_view->mapped = layer_view->layer_surface.mapped;
    viv_layers_arrange(layer_view->output);
    viv_output_mark_for_relayout(layer_view->output);
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
	layer_view->new_popup.notify = layer_surface_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &layer_view->new_popup);
    layer_view->surface_commit.notify = layer_surface_surface_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &layer_view->surface_commit);

    struct viv_output *output = viv_output_of_wlr_output(server, layer_surface->output);
    wl_list_insert(&output->layer_views, &layer_view->output_link);
    layer_view->output = output;

    // Immediately arrange the layers in order to send a configure to the new layer surface.  This
    // is essential for bemenu-run to work, otherwise it tries to mmap without having a configured
    // width/height yet.
    // TODO: Reassess this - do we really need to do all this work right now?
    viv_layers_arrange(output);
}

void viv_layers_arrange(struct viv_output *output) {
    uint32_t *margin_left = &output->excluded_margin.left;
    uint32_t *margin_right = &output->excluded_margin.right;
    uint32_t *margin_top = &output->excluded_margin.top;
    uint32_t *margin_bottom = &output->excluded_margin.bottom;

    *margin_left = 0;
    *margin_right = 0;
    *margin_top = 0;
    *margin_bottom = 0;

    struct wlr_output_layout_output *output_layout_output = wlr_output_layout_get(output->server->output_layout, output->wlr_output);
    int ox = output_layout_output->x + output->excluded_margin.left;
    int oy = output_layout_output->y + output->excluded_margin.top;

    uint32_t output_width = output->wlr_output->width;
    uint32_t output_height = output->wlr_output->height;

    // TODO: This layout function is inefficient and relies on some guesses about how the
    // layer shell is supposed to work, it needs reassessment later
    struct viv_layer_view *layer_view;
    // TODO: Iterate in an order that respects exclusion hint strength
    // TODO: Support margins
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        struct wlr_layer_surface_v1_state state = layer_view->layer_surface->current;
        bool anchor_left = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
        bool anchor_right = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        bool anchor_top = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
        bool anchor_bottom = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

        bool anchor_horiz = anchor_left && anchor_right;
        bool anchor_vert = anchor_bottom && anchor_top;

        uint32_t desired_width = state.desired_width;
        if (desired_width == 0) {
            desired_width = output_width;
        }
        uint32_t desired_height = state.desired_height;
        if (desired_height == 0) {
            desired_height = output_height;
        }

        uint32_t anchor_sum = anchor_left + anchor_right + anchor_top + anchor_bottom;
        switch (anchor_sum) {
        case 0:
            // Not anchored to any edge => display in centre with suggested size
            wlr_layer_surface_v1_configure(layer_view->layer_surface, desired_width, desired_height);
            layer_view->x = output_width / 2 - desired_width / 2;
            layer_view->y = output_height / 2 - desired_height / 2;
            break;
        case 1:
            wlr_log(WLR_ERROR, "One anchor");
            // Anchored to one edge => use suggested size
            wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                           desired_width, desired_height);
            if (anchor_left || anchor_right) {
                layer_view->y = output_height / 2 - desired_height / 2;
                if (anchor_left) {
                    layer_view->x = 0;
                } else {
                    layer_view->x = output_width - desired_width;
                }
            } else {
                layer_view->x = output_width / 2 - desired_width / 2;
                if (anchor_top) {
                    layer_view->y = 0;
                } else {
                    layer_view->y = output_height - desired_height;
                }
            }
            wlr_log(WLR_ERROR, "Set layer surface x %d, y %d, width %d, height %d",
                    layer_view->x, layer_view->y, desired_width, desired_height);
            break;
        case 2:
            // Anchored to two edges => use suggested size

            if (anchor_horiz) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = 0;
                layer_view->y = output_height / 2 - desired_height / 2;
            } else if (anchor_vert) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = output_width / 2 - desired_width / 2;
                layer_view->y = 0;
            } else if (anchor_top && anchor_left) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = 0;
                layer_view->y = 0;
            } else if (anchor_top && anchor_right) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = output_width - desired_width;
                layer_view->y = 0;
            } else if (anchor_bottom && anchor_right) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = output_width - desired_width;
                layer_view->y = output_height - desired_height;
            } else if (anchor_bottom && anchor_left) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, desired_height);
                layer_view->x = 0;
                layer_view->y = output_height - desired_height;
            }
            break;
        case 3:
            // Anchored to three edges => use suggested size on free axis only
            if (anchor_horiz) {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               output_width, desired_height);
                layer_view->x = 0;
                if (anchor_top) {
                    layer_view->y = *margin_top;
                    if (state.exclusive_zone) {
                        *margin_top += desired_height;
                    }
                } else {
                    layer_view->y = output_height - desired_height - *margin_bottom;
                    if (state.exclusive_zone) {
                        *margin_bottom += desired_height;
                    }
                }
            } else {
                wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                               desired_width, output_height);
                layer_view->y = 0;
                if (anchor_left) {
                    layer_view->x = *margin_left;
                    if (state.exclusive_zone) {
                        *margin_left += desired_width;
                    }
                } else {
                    layer_view->x = output_width - desired_width - *margin_right;
                    if (state.exclusive_zone) {
                        *margin_right += desired_width;
                    }
                }
            }
            break;
        case 4:
            // Fill the output
            wlr_layer_surface_v1_configure(layer_view->layer_surface,
                                            output_width, output_height);
            layer_view->x = 0;
            layer_view->y = 0;
            break;
        default:
            UNREACHABLE()
        }

        layer_view->x += ox;
        layer_view->y += oy;
    }
}

bool viv_layer_view_is_at(struct viv_layer_view *layer_view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
    double view_sx = lx - layer_view->x;
    double view_sy = ly - layer_view->y;
    double _sx, _sy;
    struct wlr_surface *_surface = wlr_layer_surface_v1_surface_at(layer_view->layer_surface,
                                                                  view_sx, view_sy, &_sx, &_sy);
    if (_surface != NULL) {
        *sx = _sx;
        *sy = _sy;
        *surface = _surface;
        return true;
    }

    return false;
}


bool viv_layer_view_layer_in(struct viv_layer_view *layer_view, uint32_t layers) {
    enum zwlr_layer_shell_v1_layer layer = layer_view->layer_surface->current.layer;
    if (layers & (1 << layer)) {
        return true;
    }
    return false;
}
