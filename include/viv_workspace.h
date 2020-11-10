
#ifndef VIV_WORKSPACE_H
#define VIV_WORKSPACE_H

#include "viv_types.h"

void viv_workspace_roll_windows(struct viv_workspace *workspace);

void viv_workspace_increment_divide(struct viv_workspace *workspace, float increment);

void viv_workspace_swap_out(struct viv_output *output, struct wl_list *workspaces);

#endif
