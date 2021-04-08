
#ifndef VIV_WORKSPACE_H
#define VIV_WORKSPACE_H

#include "viv_types.h"

void viv_workspace_increment_divide(struct viv_workspace *workspace, float increment);

void viv_workspace_increment_counter(struct viv_workspace *workspace, uint32_t increment);

void viv_workspace_swap_out(struct viv_output *output, struct wl_list *workspaces);

void viv_workspace_focus_next_window(struct viv_workspace *workspace);
void viv_workspace_focus_prev_window(struct viv_workspace *workspace);

void viv_workspace_shift_active_window_down(struct viv_workspace *workspace);
void viv_workspace_shift_active_window_up(struct viv_workspace *workspace);

void viv_workspace_next_layout(struct viv_workspace *workspace);
void viv_workspace_prev_layout(struct viv_workspace *workspace);

/// Set the given output's workspace to the given workspace, switching with the previous
/// one if necessary
void viv_workspace_assign_to_output(struct viv_workspace *workspace, struct viv_output *output);


/// Returns the first non-floating view in the workspace, or NULL if there is none
struct viv_view *viv_workspace_main_view(struct viv_workspace *workspace);


/// Switches the current active window with the main window from the workspace
void viv_workspace_swap_active_and_main(struct viv_workspace *workspace);

/// Apply the layout function of the workspace, and handle tidying up e.g. pointer focus
void viv_workspace_do_layout(struct viv_workspace *workspace);

/// Get the number of tiled (i.e. non-floating) views in the workspace
uint32_t viv_workspace_num_tiled_views(struct viv_workspace *workspace);

/// Add the given view to the current workspace, taking the place of the current active
/// view in the views list (if any). The `view` must not already be in any views list, its
/// link will be reused without checking.
void viv_workspace_add_view(struct viv_workspace *workspace, struct viv_view *view);

/// Mark all views in the workspace as damaged
void viv_workspace_damage_views(struct viv_workspace *workspace);

/// Mark that the workspace needs a layout - this works by damaging it
/// then, after the next draw, actually applying the new layout
// TODO: Should we just layout straight away now?
void viv_workspace_mark_for_relayout(struct viv_workspace *workspace);

#endif
