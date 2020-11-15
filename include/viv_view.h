#ifndef VIV_VIEW_H
#define VIV_VIEW_H

#include <wlr/types/wlr_surface.h>

#include "viv_types.h"

void viv_view_bring_to_front(struct viv_view *view);

void viv_view_focus(struct viv_view *view, struct wlr_surface *surface);

#endif
