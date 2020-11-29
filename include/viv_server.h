#ifndef VIV_SERVER_H
#define VIV_SERVER_H

#include "viv_types.h"

void viv_server_init(struct viv_server *server);

struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);


void viv_server_begin_interactive(struct viv_view *view, enum viv_cursor_mode mode, uint32_t edges);

struct viv_workspace *viv_server_retrieve_workspace_by_name(struct viv_server *server, char *name);

#endif
