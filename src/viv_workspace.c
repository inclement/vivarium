
#include "viv_types.h"

void viv_workspace_roll_windows(struct viv_workspace *workspace) {
}

void viv_workspace_increment_divide(struct viv_workspace *workspace, float increment) {
    workspace->divide += increment;
    if (workspace->divide > 1) {
        workspace->divide = 1;
    } else if (workspace->divide < 0) {
        workspace->divide = 0;
    }

    workspace->needs_layout = true;
}
