#ifndef VIV_VIEW_H
#define VIV_VIEW_H

#include <wlr/types/wlr_surface.h>

#include "viv_types.h"

/// Bring the given view to the front of its workspace view list
void viv_view_bring_to_front(struct viv_view *view);

/// Make the given view and surface the current focus of keyboard input, and the active
/// view in the current workspace
void viv_view_focus(struct viv_view *view, struct wlr_surface *surface);

/// Mark the given view as floating, and trigger a workspace layout update if necessary
void viv_view_ensure_floating(struct viv_view *view);

/// Mark the given view as tiled, and trigger a workspace layout update if necessary
void viv_view_ensure_tiled(struct viv_view *view);

/// Move the given view to the target workspace. If the workspace is
/// currently visible, preserve its focus, otherwise focus the next
/// view in the current workspace.
void viv_view_shift_to_workspace(struct viv_view *view, struct viv_workspace *workspace);

/// Get the next view in the current workspace (which may be the same
/// view if it's the only one present)
struct viv_view *viv_view_next_in_workspace(struct viv_view *view);

/// Get the previous view in the current workspace (which may be the same
/// view if it's the only one present)
struct viv_view *viv_view_prev_in_workspace(struct viv_view *view);

/// Send a close request to this view
void viv_view_request_close(struct viv_view *view);

/// Return a string identifying the view type, as reported by the running application
void viv_view_string_identifier(struct viv_view *view, char *buffer, size_t len);
#endif
