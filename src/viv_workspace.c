#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "viv_types.h"
#include "viv_view.h"

static void swap_list_entries(struct wl_list *elm1, struct wl_list *elm2) {
    struct wl_list *elm1_prev = elm1->prev;
    struct wl_list *elm1_next = elm1->next;

    elm1->prev = elm2->prev;
    elm1->next = elm2->next;

    elm2->prev = elm1_prev;
    elm2->next = elm1_next;

    // If the two list items were next to one another then they will have gained circular
    // references, so fix these before correcting references of adjacent elements.
    if (elm1->next == elm1) {
        elm1->next = elm2;
    }
    if (elm1->prev == elm1) {
        elm1->prev = elm2;
    }
    if (elm2->next == elm2) {
        elm2->next = elm1;
    }
    if (elm2->next == elm2) {
        elm2->next = elm1;
    }

    elm1->prev->next = elm1;
    elm1->next->prev = elm1;

    elm2->prev->next = elm2;
    elm2->next->prev = elm2;

}

void viv_workspace_next_window(struct viv_workspace *workspace) {
    struct viv_view *active_view = workspace->active_view;
    if (active_view == NULL) {
        wlr_log(WLR_DEBUG, "Could not get next window, no active view");
        return;
    }

    struct wl_list *next_link = active_view->workspace_link.next;
    if (next_link == &workspace->views) {
        next_link = next_link->next;
    }

    if (next_link == &workspace->views) {
        wlr_log(WLR_ERROR, "Next window couldn't be found\n");
        return;
    }

    struct viv_view *next_view = wl_container_of(next_link, next_view, workspace_link);

    viv_view_focus(next_view, next_view->xdg_surface->surface);
}

void viv_workspace_prev_window(struct viv_workspace *workspace) {
    struct viv_view *active_view = workspace->active_view;
    if (active_view == NULL) {
        wlr_log(WLR_DEBUG, "Could not get prev window, no active view");
        return;
    }

    struct wl_list *prev_link = active_view->workspace_link.prev;
    if (prev_link == &workspace->views) {
        prev_link = prev_link->prev;
    }

    if (prev_link == &workspace->views) {
        wlr_log(WLR_ERROR, "Prev window couldn't be found\n");
        return;
    }

    struct viv_view *prev_view = wl_container_of(prev_link, prev_view, workspace_link);

    viv_view_focus(prev_view, prev_view->xdg_surface->surface);
}

void viv_workspace_shift_active_window_up(struct viv_workspace *workspace) {
    struct viv_view *active_view = workspace->active_view;
    if (active_view == NULL) {
        wlr_log(WLR_DEBUG, "Could not get next window, no active view");
        return;
    }

    struct wl_list *prev_link = active_view->workspace_link.prev;
    if (prev_link == &workspace->views) {
        prev_link = prev_link->prev;
    }

    if (prev_link == &workspace->views) {
        wlr_log(WLR_ERROR, "Next window couldn't be found\n");
        return;
    }

    swap_list_entries(&active_view->workspace_link, prev_link);

    active_view->workspace->needs_layout = true;
}

void viv_workspace_shift_active_window_down(struct viv_workspace *workspace) {
    struct viv_view *active_view = workspace->active_view;
    if (active_view == NULL) {
        wlr_log(WLR_DEBUG, "Could not get next window, no active view");
        return;
    }

    struct wl_list *next_link = active_view->workspace_link.next;
    if (next_link == &workspace->views) {
        next_link = next_link->next;
    }

    if (next_link == &workspace->views) {
        wlr_log(WLR_ERROR, "Next window couldn't be found\n");
        return;
    }

    swap_list_entries(&active_view->workspace_link, next_link);

    active_view->workspace->needs_layout = true;
}

void viv_workspace_increment_divide(struct viv_workspace *workspace, float increment) {
    workspace->active_layout->parameter += increment;
    if (workspace->active_layout->parameter > 1) {
        workspace->active_layout->parameter = 1;
    } else if (workspace->active_layout->parameter < 0) {
        workspace->active_layout->parameter = 0;
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

    output->needs_layout = true;

    if (new_workspace->active_view != NULL) {
        viv_view_focus(new_workspace->active_view, new_workspace->active_view->xdg_surface->surface);
    }

    printf("Workspace changed from %s to %s\n", old_workspace->name, new_workspace->name);
}
