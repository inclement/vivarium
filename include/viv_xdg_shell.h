#ifndef VIV_XDG_SHELL_H
#define VIV_XDG_SHELL_H

#include "viv_types.h"

/// init and return a viv_view to represent the given xdg_surface
void viv_xdg_view_init(struct viv_view *view, struct wlr_xdg_surface *xdg_surface);


#endif
