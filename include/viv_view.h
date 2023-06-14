#ifndef VIV_VIEW_H
#define VIV_VIEW_H

#include <wlr/types/wlr_compositor.h>

#include "viv_types.h"

/// Bring the given view to the front of its workspace view list
void viv_view_bring_to_front(struct viv_view *view);

/// Clear focus from all views handled by the server;
void viv_view_clear_all_focus(struct viv_server *server);

/// Make the given view the current focus of keyboard input, and the active
/// view in the current workspace
void viv_view_focus(struct viv_view *view);

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
void viv_view_get_string_identifier(struct viv_view *view, char *buffer, size_t len);

/// True if the surface geometry size exceeds that of the target draw region, else false
bool viv_view_oversized(struct viv_view *view);

/// Set the size of a view
void viv_view_set_size(struct viv_view *view, uint32_t width, uint32_t height);

/// Set the pos of a view, in global coordinates
void viv_view_set_size(struct viv_view *view, uint32_t width, uint32_t height);

/// Get the geometry box of a view
void viv_view_get_geometry(struct viv_view *view, struct wlr_box *geo_box);

/// Perform generic initialisation of a viv_view. Should be performed before
/// shell-specific initialisation. This includes creation if a wlr_scene node, ready for
/// shell-specific initialisation to populate with one or more children.
void viv_view_init(struct viv_view *view, struct viv_server *server);

/// Add the view to the current output and set up all its state so that it's ready to
/// appear in a workspace once mapped.  Requires that shell-specific configuration has
/// already taken place.
void viv_view_add_to_output(struct viv_view *view);

/// Clear up view state, remove it from its workspace, and free its memory
void viv_view_destroy(struct viv_view *view);

/// Get the wlr_surface that is the main/toplevel surface for this view
struct wlr_surface *viv_view_get_toplevel_surface(struct viv_view *view);

/** Test if any surfaces of the given view are at the given layout coordinates, including
    nested surfaces (e.g. popup windows, tooltips).  If so, return the surface data.
    @param view Pointer to the view to test
    @param lx Position to test in layout coordinates
    @param ly Position to test in layout coordinates
    @param surface wlr_surface via which to return if found
    @param sx Surface coordinate to return (relative to surface top left corner)
    @param sy Surface coordinate to return (relative to surface top left corner)
    @returns true if a surface was found, else false
*/
bool viv_view_is_at(struct viv_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy);

/// Set the target bounding box for the view, in workspace-local coordinates. The view's
/// actual position and size will be set consistent with this demand in global
/// coordinates, with size reduced to make space for view borders and gaps.
void viv_view_set_target_box(struct viv_view *view, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/// Adjusts the target box so that it matches the surface exactly, taking into
/// account borders and gaps
void viv_view_match_target_box_with_surface_geometry(struct viv_view *view);

/// Ensure that the given view is not active: if it is active, the active view in its
/// workspace will be set to the next view (if present) or otherwise to NULL. This is
/// useful for making sure focus goes somewhere when a view is unmapped.
void viv_view_ensure_not_active_in_workspace(struct viv_view *view);

/// Sets the view's fullscreen state. Returns true if the operation was succesful, false otherwise
bool viv_view_set_fullscreen(struct viv_view *view, bool fullscreen);

/// Sets a view to non-fullscreen and informs the client if necessary
void viv_view_force_remove_fullscreen(struct viv_view *view);

void viv_view_sync_target_box_to_scene(struct viv_view *view);

#endif
