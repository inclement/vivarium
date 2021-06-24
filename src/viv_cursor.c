#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>

#include "viv_cursor.h"
#include "viv_layer_view.h"
#include "viv_output.h"
#include "viv_seat.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"

static void process_cursor_move_view(struct viv_server *server, uint32_t time) {
    UNUSED(time);

    struct viv_view *view = server->grab_state.view;

    int old_x = view->x;
    int old_y = view->y;

    // Damage before moving
    viv_view_damage(view);

	/* Move the grabbed view to the new position. */
	view->x = server->default_seat->cursor->x - server->grab_state.x;
	view->y = server->default_seat->cursor->y - server->grab_state.y;

    view->target_x += (view->x - old_x);
    view->target_y += (view->y - old_y);

    // Move the grabbed view to the new output, if necessary
	double cursor_x = server->default_seat->cursor->x;
	double cursor_y = server->default_seat->cursor->y;
    struct viv_output *output_at_point = viv_output_at(server, cursor_x, cursor_y);
    if (output_at_point != view->workspace->output) {
        viv_view_shift_to_workspace(view, output_at_point->current_workspace);
    }

    // Damage after moving
    viv_view_damage(view);
}

static void process_cursor_resize_view(struct viv_server *server, uint32_t time) {
    UNUSED(time);
    // TODO: As it stands this inherits some simplications from
    // tinywl, we should wait for a buffer at the new size before
    // committing movement
	struct viv_view *view = server->grab_state.view;

    viv_view_damage(view);

	double border_x = server->default_seat->cursor->x - server->grab_state.x;
	double border_y = server->default_seat->cursor->y - server->grab_state.y;
	int new_left = server->grab_state.geobox.x;
	int new_right = server->grab_state.geobox.x + server->grab_state.geobox.width;
	int new_top = server->grab_state.geobox.y;
	int new_bottom = server->grab_state.geobox.y + server->grab_state.geobox.height;

	if (server->grab_state.resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->grab_state.resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->grab_state.resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->grab_state.resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	view->x = new_left;
	view->y = new_top;

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
    viv_view_set_size(view, new_width, new_height);

    view->target_x = new_left;
    view->target_y = new_top;
    view->target_width = new_width;
    view->target_height = new_height;

    viv_view_damage(view);
}

static bool layer_view_wants_keyboard_focus(struct viv_layer_view *layer_view) {
    return (layer_view->layer_surface->current.keyboard_interactive);
}

/// Find the focusable surface under the pointer (if any) and pass the event data along
static void process_cursor_pass_through_to_surface(struct viv_server *server, uint32_t time) {
	double sx, sy;
	struct wlr_seat *seat = server->default_seat->wlr_seat;
	struct wlr_surface *surface = NULL;

    // TODO: This will need to iterate over views in each desktop, with some appropriate ordering
    struct viv_layer_view *layer_view;
	struct viv_view *view = NULL;

    // Find the uppermost view or layer view under the cursor
    if ((layer_view = viv_server_layer_view_at(server, server->default_seat->cursor->x, server->default_seat->cursor->y, &surface, &sx, &sy, VIV_LAYER_MASK_OVERLAY))) {
    } else if ((layer_view = viv_server_layer_view_at(server, server->default_seat->cursor->x, server->default_seat->cursor->y, &surface, &sx, &sy, VIV_LAYER_MASK_TOP))) {
    } else if ((view = viv_server_view_at(server, server->default_seat->cursor->x, server->default_seat->cursor->y, &surface, &sx, &sy))) {
    } else if ((layer_view = viv_server_layer_view_at(server, server->default_seat->cursor->x, server->default_seat->cursor->y, &surface, &sx, &sy, VIV_LAYER_MASK_BOTTOM))) {
    } else if ((layer_view = viv_server_layer_view_at(server, server->default_seat->cursor->x, server->default_seat->cursor->y, &surface, &sx, &sy, VIV_LAYER_MASK_BACKGROUND))) {
    }

    // Act appropriately on whatever view type was found
    if (layer_view) {
        if (layer_view_wants_keyboard_focus(layer_view)) {
            viv_seat_focus_surface(server->default_seat, surface);
            server->active_output->current_workspace->active_view = NULL;
        }
    } else if (view) {
        // View under the cursor and not already active => focus it if appropriate
        struct viv_view *active_view = NULL;

        struct viv_output *active_output = server->active_output;
        if (active_output) {
            active_view = active_output->current_workspace->active_view;
        }

        if ((view != active_view) && server->config->focus_follows_mouse) {
            viv_view_focus(view, surface);
        }
    } else {
        // No focusable surface under the cursor => use the default image
		wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", server->default_seat->cursor);
    }

    viv_cursor_reset_focus(server, time);
	if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface != surface;
        // Set pointer focus appropriately - note this is distinct from keyboard focus
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
			/* The enter event contains coordinates, so we only need to notify
			 * on motion if the focus did not change. */
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

/** Handle new cursor data, i.e. acting on an in-progress move or resize, or otherwise
    passing through the event to a view.
*/
void viv_cursor_process_cursor_motion(struct viv_server *server, uint32_t time) {
    // Always update the current output if necessary
	double cursor_x = server->default_seat->cursor->x;
	double cursor_y = server->default_seat->cursor->y;
    struct viv_output *output_at_point = viv_output_at(server, cursor_x, cursor_y);
    viv_output_make_active(output_at_point);

    // Respond to the specific cursor movement
    switch (server->default_seat->cursor_mode) {
    case VIV_CURSOR_MOVE:
		process_cursor_move_view(server, time);
        break;
    case VIV_CURSOR_RESIZE:
        process_cursor_resize_view(server, time);
        break;
    case VIV_CURSOR_PASSTHROUGH:
        process_cursor_pass_through_to_surface(server, time);
        break;
    }

}

/// Check for a layer view or normal view at the cursor pos, in each layer (including the
/// view layer) until one is found, or returns NULL if none is found.
static struct wlr_surface *uppermost_surface_at_cursor(struct viv_server *server, double *sx, double *sy) {

    double cursor_x = server->default_seat->cursor->x;
    double cursor_y = server->default_seat->cursor->y;
    struct wlr_surface *surface = NULL;
    if ((viv_server_layer_view_at(server, cursor_x, cursor_y, &surface, sx, sy, VIV_LAYER_MASK_OVERLAY))) {
    } else if ((viv_server_layer_view_at(server, cursor_x, cursor_y, &surface, sx, sy, VIV_LAYER_MASK_TOP))) {
    } else if ((viv_server_view_at(server, cursor_x, cursor_y, &surface, sx, sy))) {
    } else if ((viv_server_layer_view_at(server, cursor_x, cursor_y, &surface, sx, sy, VIV_LAYER_MASK_BOTTOM))) {
    } else if ((viv_server_layer_view_at(server, cursor_x, cursor_y, &surface, sx, sy, VIV_LAYER_MASK_BACKGROUND))) {
    }

    return surface;
}

void viv_cursor_reset_focus(struct viv_server *server, uint32_t time) {
    struct wlr_seat *seat = server->default_seat->wlr_seat;
	double sx, sy;
	struct wlr_surface *surface = uppermost_surface_at_cursor(server, &sx, &sy);

	if (surface) {
		bool focus_changed = seat->pointer_state.focused_surface != surface;
        // Set pointer focus appropriately - note this is distinct from keyboard focus
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		if (!focus_changed) {
			/* The enter event contains coordinates, so we only need to notify
			 * on motion if the focus did not change. */
			wlr_seat_pointer_notify_motion(seat, time, sx, sy);
		}
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}
