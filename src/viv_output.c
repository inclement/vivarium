#include "viv_output.h"

#include "viv_types.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>

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
