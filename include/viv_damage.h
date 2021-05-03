#ifndef VIV_DAMAGE_H
#define VIV_DAMAGE_H

#include "viv_types.h"

/// Damage the given surface, which is placed at the given layout coords, on every output
void viv_damage_surface(struct viv_server *server, struct wlr_surface *surface, int lx, int ly);

#endif
