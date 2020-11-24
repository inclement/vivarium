#ifndef VIV_VIEW_H
#define VIV_VIEW_H

#include <wlr/types/wlr_surface.h>

#include "viv_types.h"

/// Bring the given view to the front of its workspace view list
void viv_view_bring_to_front(struct viv_view *view);

/// MAke the given view and surface the current focus of keyboard input, and the active
/// view in the current workspace
void viv_view_focus(struct viv_view *view, struct wlr_surface *surface);

/// Mark the given view as floating, and trigger a workspace layout update if necessary
void viv_view_ensure_floating(struct viv_view *view);

#endif
