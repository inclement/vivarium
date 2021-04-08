#ifndef VIV_RENDER_H
#define VIV_RENDER_H

#include "viv_types.h"

void viv_render_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output, pixman_region32_t *damage);

void viv_render_layer_view(struct wlr_renderer *renderer, struct viv_layer_view *layer_view, struct viv_output *output);

/// Render all surfaces on the given output, in appropriate order
void viv_render_output(struct wlr_renderer *renderer, struct viv_output *output);
#endif
