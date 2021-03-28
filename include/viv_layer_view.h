#ifndef VIV_LAYER_VIEW_H
#define VIV_LAYER_VIEW_H

#include "viv_types.h"
#include <wlr/types/wlr_layer_shell_v1.h>
#include "wlr-layer-shell-unstable-v1-protocol.h"

enum viv_layer_mask {
    VIV_LAYER_MASK_BACKGROUND = 1 << ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
    VIV_LAYER_MASK_BOTTOM = 1 << ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
    VIV_LAYER_MASK_TOP = 1 << ZWLR_LAYER_SHELL_V1_LAYER_TOP,
    VIV_LAYER_MASK_OVERLAY = 1 << ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
};

#define VIV_ANY_LAYER (VIV_LAYER_MASK_BACKGROUND | VIV_LAYER_MASK_BOTTOM | VIV_LAYER_MASK_TOP | VIV_LAYER_MASK_OVERLAY)

/// Initialise a new layer view struct
void viv_layer_view_init(struct viv_layer_view *view, struct viv_server *server, struct wlr_layer_surface_v1 *layer_surface);

/// Arrange the layer views on the given output according to their properties, and set
/// excluded regions appropriately.
void viv_layers_arrange(struct viv_output *output);

/// Test if any surfaces of the given layer view are at the given layout coordinates, including
/// nested surfaces (e.g. popup windows, tooltips). If so, return the surface data.
bool viv_layer_view_is_at(struct viv_layer_view *layer_view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy);

/// Test if the given layer view is in any of the queried layers
bool viv_layer_view_layer_in(struct viv_layer_view *layer_view, uint32_t layers);

#endif
