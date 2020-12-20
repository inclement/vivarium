#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>

#include "viv_output.h"

#include "viv_cursor.h"
#include "viv_render.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_workspace.h"

/// Handle a render frame event: render everything on the output, then do any scheduled relayouts
static void output_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);

    // This has been called because a specific output is ready to display a frame,
    // retrieve this info
	struct viv_output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

#ifdef DEBUG
    viv_check_data_consistency(output->server);
#endif

    viv_render_output(renderer, output);

    // If the workspace has been been relayout recently, reset the pointer focus just in
    // case surfaces have changed size since the last frame
    // TODO: There must be a better way to do this
    struct viv_workspace *workspace = output->current_workspace;
    if (workspace->was_laid_out) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        viv_cursor_reset_focus(workspace->output->server, (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000);
        workspace->was_laid_out = false;
    }

    // TODO this probably shouldn't be here?  For now do layout right after committing a
    // frame, to give time for clients to re-draw before the next one. There's probably a
    // better way to do this.
    viv_workspace_do_layout_if_necessary(output->current_workspace);
}

struct viv_output *viv_output_at(struct viv_server *server, double lx, double ly) {

    struct wlr_output *wlr_output_at_point = wlr_output_layout_output_at(server->output_layout, lx, ly);

    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output == wlr_output_at_point) {
            return output;
        }
    }
    return NULL;
}

void viv_output_make_active(struct viv_output *output) {
    if (output == NULL) {
        // It's acceptable to be asked to make a NULL output active
        // TODO: is it really?
        return;
    }

    output->server->active_output = output;
}

struct viv_output *viv_output_of_wlr_output(struct viv_server *server, struct wlr_output *wlr_output) {
    struct viv_output *output = NULL;
    wl_list_for_each(output, &server->outputs, link) {
        if (output->wlr_output == wlr_output) {
            return output;
        }
    }
    return NULL;
}


struct viv_output *viv_output_next_in_direction(struct viv_output *output, enum wlr_direction direction) {
    struct viv_server *server = output->server;
    struct wlr_output *cur_wlr_output = output->wlr_output;
    struct wlr_output_layout_output *cur_wlr_output_layout_output = wlr_output_layout_get(server->output_layout,
                                                                                          cur_wlr_output);

    struct wlr_output *new_wlr_output = wlr_output_layout_adjacent_output(
        server->output_layout,
        direction,
        cur_wlr_output,
        cur_wlr_output_layout_output->x,
        cur_wlr_output_layout_output->y);

    struct viv_output *new_output = viv_output_of_wlr_output(server, new_wlr_output);

    return new_output;
}

void viv_output_display_workspace(struct viv_output *output, struct viv_workspace *workspace) {

    struct viv_output *other_output = workspace->output;

    if (other_output == output) {
        wlr_log(WLR_DEBUG, "Cannot switch to workspace %s, it is already being displayed on this output",
                workspace->name);
        return;
    }

    if (other_output) {
        other_output->current_workspace = output->current_workspace;
        other_output->current_workspace->output = other_output;
        other_output->needs_layout = true;
    } else {
        output->current_workspace->output = NULL;
    }

    output->current_workspace = workspace;
    output->current_workspace->output = output;
    output->needs_layout = true;
}

void viv_output_init(struct viv_output *output, struct viv_server *server, struct wlr_output *wlr_output) {
    wl_list_init(&output->layer_views);

	output->wlr_output = wlr_output;
	output->server = server;
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);
    wlr_log(WLR_ERROR, "New output at %p", output);

    struct viv_workspace *current_workspace;
    wl_list_for_each(current_workspace, &server->workspaces, server_link) {
        if (current_workspace->output == NULL) {
            wlr_log(WLR_INFO, "Assigning new output workspace %s", current_workspace->name);
            viv_workspace_assign_to_output(current_workspace, output);
            break;
        }
    }

	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
