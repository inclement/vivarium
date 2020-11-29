#ifndef VIV_XDG_SHELL_H
#define VIV_XDG_SHELL_H

#include "viv_types.h"

/// Create, init and return a viv_view to represent the given xdg_surface
struct viv_view *viv_xdg_view_create(struct viv_server *server, struct wlr_xdg_surface *xdg_surface);

#endif
