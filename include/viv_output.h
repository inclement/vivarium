#ifndef VIV_OUTPUT_H
#define VIV_OUTPUT_H

#include <wlr/types/wlr_output_layout.h>

#include "viv_types.h"

struct viv_output *viv_output_at(struct viv_server *server, double lx, double ly);

void viv_output_make_active(struct viv_output *output);

struct viv_output *viv_output_of_wlr_output(struct viv_server *server, struct wlr_output *wlr_output);

struct viv_output *viv_output_next_in_direction(struct viv_output *output, enum wlr_direction direction);

/// Display the given workspace on the given output. If the given workspace was previously
/// displayed on another output, swap with that one.
void viv_output_display_workspace(struct viv_output *output, struct viv_workspace *workspace);

/// Initialise a new viv_output
void viv_output_init(struct viv_output *output, struct viv_server *server, struct wlr_output *wlr_output);

/// Apply the layout function of the current workspace, but only if the output or current
/// workspace need layouting
void viv_output_do_layout_if_necessary(struct viv_output *output);
#endif
