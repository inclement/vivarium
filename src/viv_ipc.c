#include <signal.h>
#include <stdio.h>

#include <wayland-util.h>
#include <wlr/util/log.h>

#include "viv_types.h"

static void write_workspaces_file(char *output_filen, struct wl_list *workspaces, struct viv_output *active_output) {
    FILE *handle = fopen(output_filen, "w");
    if (handle == NULL) {
        wlr_log(WLR_ERROR, "Error opening file \"%s\", skipping", output_filen);
        return;
    }

    fprintf(handle, " ");

    struct viv_workspace *workspace;
    wl_list_for_each(workspace, workspaces, server_link) {
        if (workspace->output == NULL) {
            fprintf(handle, "%s ", workspace->name);
        } else if (workspace->output == active_output) {
            fprintf(handle, "<%s> ", workspace->name);
        } else {
            fprintf(handle, "[%s] ", workspace->name);
        }
    }

    fprintf(handle, "\n");

    fclose(handle);
}


void viv_routine_log_state(struct viv_server *server) {
    char *output_filen = server->config->ipc_workspaces_filename;
    if (output_filen == NULL) {
        // No output file configured, so just do nothing
        return;
    }

    if ((server->log_state.last_active_output == server->active_output) &&
        (server->log_state.last_active_workspace == server->active_output->current_workspace)) {
        // Nothing loggable has changed
        return;
    }

    server->log_state.last_active_output = server->active_output;
    server->log_state.last_active_workspace = server->active_output->current_workspace;

    write_workspaces_file(output_filen, &server->workspaces, server->active_output);

    // If configured, send a signal to the bar program to let it know it should update
    if (server->bar_pid && server->config->bar.update_signal_number) {
        kill(server->bar_pid, SIGRTMIN + (int)server->config->bar.update_signal_number);
    }
}
