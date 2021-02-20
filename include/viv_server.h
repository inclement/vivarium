#ifndef VIV_SERVER_H
#define VIV_SERVER_H

#include "viv_types.h"

void viv_server_init(struct viv_server *server);

struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);


void viv_server_begin_interactive(struct viv_view *view, enum viv_cursor_mode mode, uint32_t edges);

struct viv_workspace *viv_server_retrieve_workspace_by_name(struct viv_server *server, char *name);

/// Check through all data held by the server to check for consistency, e.g. all
/// view->workspace is equal to the workspace of their workspace_link. This is intended
/// for debugging only, failures just result in logged errors.
void viv_check_data_consistency(struct viv_server *server);

/// Give keyboard focus to the given surface
void viv_surface_focus(struct viv_server *server, struct wlr_surface *surface);

/// Reload the TOML config file
void viv_server_reload_config(struct viv_server *server);
#endif
