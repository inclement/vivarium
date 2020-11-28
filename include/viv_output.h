#ifndef VIV_OUTPUT_H
#define VIV_OUTPUT_H

#include "viv_types.h"

struct viv_output *viv_output_at(struct viv_server *server, double lx, double ly);

void viv_output_make_active(struct viv_output *output);

#endif
