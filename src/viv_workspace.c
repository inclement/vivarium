
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

void viv_workspace_swap_out(struct viv_output *output, struct wl_list *workspaces) {
    if (wl_list_length(workspaces) < 2) {
        printf("Not switching workspaces as only %d present\n", wl_list_length(workspaces));
        return;
    }

    struct wl_list *new_workspace_link = workspaces->prev;
    struct viv_workspace *new_workspace = wl_container_of(new_workspace_link, new_workspace, server_link);
    struct viv_workspace *old_workspace = wl_container_of(workspaces->next, old_workspace, server_link);

    wl_list_remove(new_workspace_link);
    wl_list_insert(workspaces, new_workspace_link);

    old_workspace->output = NULL;
    new_workspace->output = output;
    output->current_workspace = new_workspace;

    output->needs_layout = 1;

    printf("Workspace changed from %s to %s\n", old_workspace->name, new_workspace->name);
}

void viv_workspace_order_view_as_focused(struct viv_view *view) {
    struct viv_workspace *workspace = view->workspace;
    if (!view->is_floating) {
        // Nothing to do, non-floating windows are tiled to not overlap.
        return;
    }

    // Bring the floating window to the front
    wl_list_remove(&view->workspace_link);
    wl_list_insert(&workspace->views, &view->workspace_link);
}
