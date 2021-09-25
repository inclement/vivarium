#include <math.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>

#include "viv_output.h"

#include "viv_cursor.h"
#include "viv_ipc.h"
#include "viv_layer_view.h"
#include "viv_render.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"
#include "viv_workspace.h"

/// Start using the output, i.e. add it to our output layout and draw a workspace on it
static void start_using_output(struct viv_output *output) {
    struct viv_server *server = output->server;
    wl_list_insert(&server->outputs, &output->link);

    struct viv_workspace *current_workspace;
    wl_list_for_each(current_workspace, &server->workspaces, server_link) {
        if (current_workspace->output == NULL) {
            wlr_log(WLR_INFO, "Assigning new output workspace %s", current_workspace->name);
            viv_workspace_assign_to_output(current_workspace, output);
            break;
        }
    }

	wlr_output_layout_add_auto(server->output_layout, output->wlr_output);
}

/// Remove the output from our output layout, revoke its workspace assignation, and clean
/// any other references to it.
static void stop_using_output(struct viv_output *output) {
    struct viv_server *server = output->server;
    wl_list_remove(&output->link);

    if (server->active_output == output) {
        server->active_output = NULL;

        // Set a new active output if any are available
        struct viv_output *new_active_output;
        wl_list_for_each(new_active_output, &server->outputs, link) {
            viv_output_make_active(new_active_output);
            break;
        }
    }

    if (server->log_state.last_active_output == output) {
        server->log_state.last_active_output = NULL;
    }

    struct viv_workspace *current_workspace;
    wl_list_for_each(current_workspace, &server->workspaces, server_link) {
        if (current_workspace->output == output) {
            wlr_log(WLR_INFO, "Clearing output for workspace %s", current_workspace->name);
            current_workspace->output = NULL;
            break;
        }
    }
    output->current_workspace = NULL;

    wlr_output_layout_remove(server->output_layout, output->wlr_output);

    // Clean up layer views last, to ensure that none of the cleanup tries to access
    // still-initialised output state
    struct viv_layer_view *layer_view;
    wl_list_for_each(layer_view, &output->layer_views, output_link){
        layer_view->output = NULL;
        wlr_layer_surface_v1_destroy(layer_view->layer_surface);
    }

}

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
        viv_cursor_reset_focus(workspace->server, (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000);
        workspace->was_laid_out = false;
    }

    // TODO this probably shouldn't be here?  For now do layout right after committing a
    // frame, to give time for clients to re-draw before the next one. There's probably a
    // better way to do this.
    viv_output_do_layout_if_necessary(output);

    viv_routine_log_state(output->server);
}

static void output_damage_event(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_output *output = wl_container_of(listener, output, damage_event);
    wlr_log(WLR_INFO, "Output \"%s\" event: damage", output->wlr_output->name);
}

static void output_present(struct wl_listener *listener, void *data) {
    UNUSED(listener);
    UNUSED(data);
}

static void output_enable(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_output *output = wl_container_of(listener, output, enable);

    wlr_log(WLR_INFO, "Output \"%s\" event: enable became %d", output->wlr_output->name, output->wlr_output->enabled);
}

static void output_mode(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_output *output = wl_container_of(listener, output, mode);
    wlr_log(WLR_INFO, "Output \"%s\" event: mode", output->wlr_output->name);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_output *output = wl_container_of(listener, output, destroy);
    wlr_log(WLR_INFO, "Output \"%s\" event: destroy", output->wlr_output->name);

    stop_using_output(output);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->damage_event.link);
    wl_list_remove(&output->present.link);
    wl_list_remove(&output->enable.link);
    wl_list_remove(&output->mode.link);
    wl_list_remove(&output->destroy.link);

    free(output);
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

    if (output->server->active_output == output) {
        // Nothing to do
        return;
    }

    if (output->server->active_output) {
        viv_output_damage(output->server->active_output);
    }
    output->server->active_output = output;
    viv_output_damage(output->server->active_output);

    if (output->current_workspace->active_view) {
        viv_view_focus(output->current_workspace->active_view, NULL);
    }
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

    if (other_output != NULL) {
        other_output->current_workspace = output->current_workspace;
        other_output->current_workspace->output = other_output;
        viv_output_mark_for_relayout(other_output);
    } else {
        output->current_workspace->output = NULL;
    }

    if (workspace->active_view) {
        viv_view_focus(workspace->active_view, NULL);
    } else {
        viv_view_clear_all_focus(output->server);
    }

    output->current_workspace = workspace;
    output->current_workspace->output = output;
    viv_output_mark_for_relayout(output);
}

void viv_output_init(struct viv_output *output, struct viv_server *server, struct wlr_output *wlr_output) {
    wl_list_init(&output->layer_views);

	output->wlr_output = wlr_output;
	output->server = server;

    output->excluded_margin.top = 0;
    output->excluded_margin.bottom = 0;
    output->excluded_margin.left = 0;
    output->excluded_margin.right = 0;

    output->damage = wlr_output_damage_create(output->wlr_output);

	output->frame.notify = output_frame;
	wl_signal_add(&output->damage->events.frame, &output->frame);

	output->damage_event.notify = output_damage_event;
	wl_signal_add(&output->wlr_output->events.damage, &output->damage_event);
	output->present.notify = output_present;
	wl_signal_add(&output->wlr_output->events.present, &output->present);
	output->enable.notify = output_enable;
	wl_signal_add(&output->wlr_output->events.enable, &output->enable);
	output->mode.notify = output_mode;
	wl_signal_add(&output->wlr_output->events.mode, &output->mode);
	output->destroy.notify = output_destroy;
	wl_signal_add(&output->wlr_output->events.destroy, &output->destroy);

    wlr_log(WLR_INFO, "New output width width %d, height %d", wlr_output->width, wlr_output->height);

    start_using_output(output);
}

void viv_output_do_layout_if_necessary(struct viv_output *output) {
    struct viv_workspace *workspace = output->current_workspace;
    if (!(output->needs_layout | workspace->needs_layout)) {
        return;
    }

    viv_layers_arrange(output);
    viv_workspace_do_layout(workspace);
}

void viv_output_damage(struct viv_output *output) {
    if (!output) {
        wlr_log(WLR_ERROR, "Tried to damage NULL output");
        return;
    }
    wlr_output_damage_add_whole(output->damage);
}

void viv_output_damage_layout_coords_box(struct viv_output *output, struct wlr_box *box) {
    struct wlr_box scaled_box;
    memcpy(&scaled_box, box, sizeof(struct wlr_box));

    double lx = 0, ly = 0;
    wlr_output_layout_output_coords(output->server->output_layout, output->wlr_output, &lx, &ly);

    scaled_box.x += lx;
    scaled_box.y += ly;

    float scale = output->wlr_output->scale;

    // TODO: Can we just round these rather than floor/ceil? Need to think through how
    // scaling actually works out on different outputs
    scaled_box.x = floor(scaled_box.x * scale);
    scaled_box.y = floor(scaled_box.y * scale);
    scaled_box.width = ceil((scaled_box.x + scaled_box.width) * scale) - floor(scaled_box.x * scale);
    scaled_box.height = ceil((scaled_box.y + scaled_box.height) * scale) - floor(scaled_box.y * scale);

    wlr_output_damage_add_box(output->damage, &scaled_box);
}

void viv_output_damage_layout_coords_region(struct viv_output *output, pixman_region32_t *damage) {
    double lx = 0, ly = 0;
    wlr_output_layout_output_coords(output->server->output_layout, output->wlr_output, &lx, &ly);

    // Shift to output coords to apply damage
    pixman_region32_translate(damage, lx, ly);

    wlr_output_damage_add(output->damage, damage);

    // Shift back to avoid permanently changing the damage
    pixman_region32_translate(damage, -lx, -ly);
}

void viv_output_layout_coords_box_to_output_coords(struct viv_output *output, struct wlr_box *geo_box) {
    double lx = 0, ly = 0;
    wlr_output_layout_output_coords(output->server->output_layout, output->wlr_output, &lx, &ly);
    geo_box->x += lx;
    geo_box->y += ly;
}

void viv_output_mark_for_relayout(struct viv_output *output) {
    if (output) {
        // The layout will be applied after the next frame
        output->needs_layout = true;
        viv_output_damage(output);
    } else {
        wlr_log(WLR_ERROR, "Tried to mark NULL output for relayout");
    }
}
