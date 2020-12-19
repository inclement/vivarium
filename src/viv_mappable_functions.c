#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

#include "viv_mappable_functions.h"

#include "viv_output.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"

void viv_mappable_do_exec(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(workspace);
    wlr_log(WLR_DEBUG, "Mappable do_exec %s\n", payload.do_exec.executable);

    pid_t p;
    if ((p = fork()) == 0) {
        setsid();
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execvp(payload.do_exec.executable, payload.do_exec.args);
        _exit(EXIT_FAILURE);
    }
}

void viv_mappable_do_shell(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(workspace);
    wlr_log(WLR_DEBUG, "Mappable do_shell %s\n", payload.do_shell.command);

    pid_t p;
    if ((p = fork()) == 0) {
        setsid();
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", payload.do_shell.command, (void *)NULL);
        _exit(EXIT_FAILURE);
    }
}

void viv_mappable_increment_divide(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    wlr_log(WLR_DEBUG, "Mappable increment divide by %f\n", payload.increment_divide.increment);
    viv_workspace_increment_divide(workspace, payload.increment_divide.increment);
}

void viv_mappable_terminate(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable terminate");
    wl_display_terminate(workspace->output->server->wl_display);
}

void viv_mappable_swap_out(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable swap_out");
    viv_workspace_swap_out(workspace->output, &workspace->output->server->workspaces);
}

void viv_mappable_next_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable next_window");
    viv_workspace_focus_next_window(workspace);
}

void viv_mappable_prev_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable prev_window");
    viv_workspace_focus_prev_window(workspace);
}

void viv_mappable_shift_active_window_down(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable shift_window_down");
    viv_workspace_shift_active_window_down(workspace);
}

void viv_mappable_shift_active_window_up(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable shift_window_up");
    viv_workspace_shift_active_window_up(workspace);
}

void viv_mappable_tile_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable tile_window");

    struct viv_view *view = workspace->active_view;

    if (!view->is_floating) {
        wlr_log(WLR_DEBUG, "Cannot tile active view, it is not floating");
        return;
    }

    bool any_not_floating = false;
    struct viv_view *non_floating_view;
    wl_list_for_each(non_floating_view, &workspace->views, workspace_link) {
        if (!non_floating_view->is_floating) {
            any_not_floating = true;
            break;
        }
    }

    viv_view_ensure_tiled(view);

    wl_list_remove(&view->workspace_link);
    if (any_not_floating) {
        // Insert right before the first non-floating view
        wl_list_insert(non_floating_view->workspace_link.prev, &view->workspace_link);
    } else {
        // Move to the end of the views (as all are floating)
        wl_list_insert(workspace->views.prev, &view->workspace_link);
    }
}

void viv_mappable_next_layout(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable next_layout");
    viv_workspace_next_layout(workspace);
}

void viv_mappable_prev_layout(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable prev_layout");
    viv_workspace_prev_layout(workspace);
}

void viv_mappable_user_function(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    wlr_log(WLR_DEBUG, "Mappable user_function");

    // Pass through the call to the user-provided function, but without the pointless payload argument
    (*payload.user_function.function)(workspace);
}

void viv_mappable_right_output(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable right_output");
    struct viv_output *cur_output = workspace->output;

    struct viv_output *next_output = viv_output_next_in_direction(cur_output, WLR_DIRECTION_RIGHT);
    viv_output_make_active(next_output);
}

void viv_mappable_left_output(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable left_output");
    struct viv_output *cur_output = workspace->output;

    struct viv_output *next_output = viv_output_next_in_direction(cur_output, WLR_DIRECTION_LEFT);
    viv_output_make_active(next_output);
}

void viv_mappable_shift_active_window_to_right_output(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable shift_active_window_to_right_output");
    struct viv_output *cur_output = workspace->output;

    struct viv_output *next_output = viv_output_next_in_direction(cur_output, WLR_DIRECTION_RIGHT);

    if (next_output) {
        viv_view_shift_to_workspace(workspace->active_view, next_output->current_workspace);
    } else {
        wlr_log(WLR_DEBUG, "Asked to shift to left output but couldn't find one in that direction");
    }
}

void viv_mappable_shift_active_window_to_left_output(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable shift_active_window_to_left_output");
    struct viv_output *cur_output = workspace->output;

    struct viv_output *next_output = viv_output_next_in_direction(cur_output, WLR_DIRECTION_LEFT);

    if (next_output) {
        viv_view_shift_to_workspace(workspace->active_view, next_output->current_workspace);
    } else {
        wlr_log(WLR_DEBUG, "Asked to shift to left output but couldn't find one in that direction");
    }
}

void viv_mappable_shift_active_window_to_workspace(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    char *name = payload.shift_active_window_to_workspace.workspace_name;
    wlr_log(WLR_DEBUG, "Mappable shift_active_window_to_workspace with name %s", name);

    struct viv_output *cur_output = workspace->output;
    struct viv_view *cur_view = workspace->active_view;

    struct viv_workspace *target_workspace = viv_server_retrieve_workspace_by_name(cur_output->server, name);
    ASSERT(target_workspace != NULL);

    viv_view_shift_to_workspace(cur_view, target_workspace);
}

void viv_mappable_switch_to_workspace(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(workspace);
    UNUSED(payload);
}

void viv_mappable_close_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    struct viv_view *view = workspace->active_view;
    viv_view_request_close(view);
}

void viv_mappable_make_window_main(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    viv_workspace_swap_active_and_main(workspace);
}
