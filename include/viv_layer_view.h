#ifndef VIV_LAYER_VIEW_H
#define VIV_LAYER_VIEW_H

#include "viv_types.h"
#include <wlr/types/wlr_layer_shell_v1.h>

/// Initialise a new layer view struct
void viv_layer_view_init(struct viv_layer_view *view, struct viv_server *server, struct wlr_layer_surface_v1 *layer_surface);

/// Arrange the layer views on the given output according to their properties, and set
/// excluded regions appropriately.
void viv_layers_arrange(struct viv_output *output);

#endif
