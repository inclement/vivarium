#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wayland-util.h>
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

void viv_mappable_increment_counter(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    wlr_log(WLR_DEBUG, "Mappable increment counter by %d\n", payload.increment_counter.increment);
    viv_workspace_increment_counter(workspace, payload.increment_counter.increment);
}

void viv_mappable_terminate(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable terminate");
    wl_display_terminate(workspace->server->wl_display);
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
    if (view == NULL) {
        wlr_log(WLR_DEBUG, "Cannot tile active view, no view is active");
        return;
    }

    if (!view->is_floating) {
        wlr_log(WLR_DEBUG, "Cannot tile active view, it is not floating");
        return;
    }

    viv_view_damage(view);

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

    viv_workspace_mark_for_relayout(workspace);
}

void viv_mappable_float_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable float_window");

    struct viv_view *view = workspace->active_view;
    if (view == NULL) {
        wlr_log(WLR_DEBUG, "Cannot float active view, no view is active");
        return;
    }

    if (view->is_floating) {
        wlr_log(WLR_DEBUG, "Cannot float active view, it is already floating");
        return;
    }

    if (view->is_fullscreen) {
        wlr_log(WLR_DEBUG, "Cannot float active view, it is fullscreen");
        return;
    }

    viv_view_damage(view);

    wl_list_remove(&view->workspace_link);

    bool any_not_floating = false;
    struct viv_view *non_floating_view;
    wl_list_for_each(non_floating_view, &workspace->views, workspace_link) {
        if (!non_floating_view->is_floating) {
            any_not_floating = true;
            break;
        }
    }

    viv_view_ensure_floating(view);

    if (any_not_floating) {
        // Insert right before the first non-floating view
        wl_list_insert(non_floating_view->workspace_link.prev, &view->workspace_link);
    } else {
        // Move to the start of the views (as all are floating)
        wl_list_insert(workspace->views.next, &view->workspace_link);
    }

    viv_workspace_mark_for_relayout(workspace);
}

void viv_mappable_toggle_floating(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    wlr_log(WLR_DEBUG, "Mappable toggle_floating");
    struct viv_view *view = workspace->active_view;
    if (!view) {
        wlr_log(WLR_DEBUG, "Cannot toggle floating: no view is active");
        return;
    }

    if (view->is_floating) {
        viv_mappable_tile_window(workspace, payload);
    } else {
        viv_mappable_float_window(workspace, payload);
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
    struct viv_view *view = workspace->active_view;

    if (!next_output) {
        wlr_log(WLR_DEBUG, "Asked to shift to left output but couldn't find one in that direction");
    } else if (!view) {
        wlr_log(WLR_DEBUG, "No active view to shift");
    } else {
        viv_view_shift_to_workspace(workspace->active_view, next_output->current_workspace);
    }
}

void viv_mappable_shift_active_window_to_left_output(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_log(WLR_DEBUG, "Mappable shift_active_window_to_left_output");
    struct viv_output *cur_output = workspace->output;

    struct viv_output *next_output = viv_output_next_in_direction(cur_output, WLR_DIRECTION_LEFT);
    struct viv_view *view = workspace->active_view;

    if (!next_output) {
        wlr_log(WLR_DEBUG, "Asked to shift to left output but couldn't find one in that direction");
    } else if (!view) {
        wlr_log(WLR_DEBUG, "No active view to shift");
    } else {
        viv_view_shift_to_workspace(workspace->active_view, next_output->current_workspace);
    }
}

void viv_mappable_shift_active_window_to_workspace(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    char *name = payload.shift_active_window_to_workspace.workspace_name;
    wlr_log(WLR_DEBUG, "Mappable shift_active_window_to_workspace with name %s", name);

    struct viv_output *cur_output = workspace->output;
    struct viv_view *cur_view = workspace->active_view;

    if (!cur_view) {
        wlr_log(WLR_DEBUG, "No active view, cannot shift to workspace %s", name);
        return;
    }

    struct viv_workspace *target_workspace = viv_server_retrieve_workspace_by_name(cur_output->server, name);
    ASSERT(target_workspace != NULL);

    viv_view_shift_to_workspace(cur_view, target_workspace);
}

void viv_mappable_switch_to_workspace(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(workspace);

    struct viv_server *server = workspace->server;

    char *workspace_name = payload.switch_to_workspace.workspace_name;
    struct viv_workspace *target_workspace = viv_server_retrieve_workspace_by_name(server, workspace_name);

    viv_output_display_workspace(workspace->output, target_workspace);
}

void viv_mappable_close_window(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    struct viv_view *view = workspace->active_view;
    if (!view) {
        wlr_log(WLR_DEBUG, "Cannot close window, active window is NULL");
        return;
    }
    viv_view_request_close(view);
}

void viv_mappable_make_window_main(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    viv_workspace_swap_active_and_main(workspace);
}

void viv_mappable_reload_config(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    struct viv_server *server = workspace->server;
    viv_server_reload_config(server);
}

void viv_mappable_debug_damage_all(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    struct viv_server *server = workspace->server;

    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        viv_output_damage(output);
    }
}

void viv_mappable_debug_swap_buffers(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    wlr_output_commit(workspace->output->wlr_output);
}

void viv_mappable_debug_toggle_show_undamaged_regions(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    workspace->server->config->debug_mark_undamaged_regions = !workspace->server->config->debug_mark_undamaged_regions;
    struct viv_output *output;
    wl_list_for_each(output, &workspace->server->outputs, link) {
        viv_output_damage(output);
    }
}

void viv_mappable_debug_toggle_mark_frame_draws(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    workspace->server->config->debug_mark_frame_draws = !workspace->server->config->debug_mark_frame_draws;
}

void viv_mappable_debug_next_damage_tracking_mode(struct viv_workspace *workspace, union viv_mappable_payload payload) {
    UNUSED(payload);
    struct viv_config *config = workspace->server->config;

    config->damage_tracking_mode++;
    if (config->damage_tracking_mode == VIV_DAMAGE_TRACKING_MAX) {
        config->damage_tracking_mode = (enum viv_damage_tracking_mode)0;
    }
}
