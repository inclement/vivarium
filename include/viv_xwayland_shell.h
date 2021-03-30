#ifndef VIV_XWAYLAND_SHELL_H
#define VIV_XWAYLAND_SHELL_H

#include "viv_types.h"

/// Initialise a new view with the given xwayland surface
void viv_xwayland_view_init(struct viv_view *view, struct wlr_xwayland_surface *xwayland_surface);

// Create an xcb connection and look up the xcb atom uints for all the
// X properties we care about, for access later
void viv_xwayland_lookup_atoms(struct viv_server *server);

#endif
