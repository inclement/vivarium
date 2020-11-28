#ifndef VIV_SERVER_H
#define VIV_SERVER_H

#include "viv_types.h"

void viv_server_init(struct viv_server *server);

struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

#endif
