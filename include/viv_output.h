#ifndef VIV_OUTPUT_H
#define VIV_OUTPUT_H

#include <wlr/types/wlr_output_layout.h>

#include "viv_types.h"

struct viv_output *viv_output_at(struct viv_server *server, double lx, double ly);

void viv_output_make_active(struct viv_output *output);

struct viv_output *viv_output_of_wlr_output(struct viv_server *server, struct wlr_output *wlr_output);

struct viv_output *viv_output_next_in_direction(struct viv_output *output, enum wlr_direction direction);

#endif
