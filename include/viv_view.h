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

/// True if the surface geometry size exceeds that of the target draw region, else false
bool viv_view_oversized(struct viv_view *view);

/// Make the given view the active view within its workspace
void viv_view_make_active(struct viv_view *view);

/// Set the size of a view
void viv_view_set_size(struct viv_view *view, uint32_t width, uint32_t height);

/// Get the geometry box of a view
void viv_view_get_geometry(struct viv_view *view, struct wlr_box *geo_box);

/// Perform generic initialisation of a viv_view. Requires that shell-specific
/// configuration has already taken place.
void viv_view_init(struct viv_view *view, struct viv_server *server);

/// Clear up view state, remove it from its workspace, and free its memory
void viv_view_destroy(struct viv_view *view);

#endif
