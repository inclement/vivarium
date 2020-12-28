#include <stdio.h>

#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "viv_view.h"

#include "viv_types.h"
#include "viv_wl_list_utils.h"

#define VIEW_NAME_LEN 100

void viv_view_bring_to_front(struct viv_view *view) {
    struct wl_list *link = &view->workspace_link;
    wl_list_remove(link);
    wl_list_insert(&view->workspace->views, link);
}

void viv_view_focus(struct viv_view *view, struct wlr_surface *surface) {
	if (view == NULL) {
		return;
	}
	struct viv_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(
					seat->keyboard_state.focused_surface);
		wlr_xdg_toplevel_set_activated(previous, false);
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

	/* Activate the new surface */
	wlr_xdg_toplevel_set_activated(view->xdg_surface, true);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(seat, view->xdg_surface->surface,
		keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

    view->workspace->active_view = view;
}

void viv_view_ensure_floating(struct viv_view *view) {
    if (!view->is_floating) {
        // Trigger a relayout only if tiling state is changing
        view->workspace->needs_layout = true;
    }
    view->is_floating = true;

    /* // Tell the view it doesn't need to worry about tiling */
    /* wlr_xdg_toplevel_set_tiled(view->xdg_surface, 0u); */
}

void viv_view_ensure_tiled(struct viv_view *view) {
    if (view->is_floating) {
        // Trigger a relayout only if tiling state is changing
        view->workspace->needs_layout = true;
    }
    view->is_floating = false;

    uint32_t all_edges = WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT | WLR_EDGE_LEFT;
    wlr_xdg_toplevel_set_tiled(view->xdg_surface, all_edges);
}

void viv_view_shift_to_workspace(struct viv_view *view, struct viv_workspace *workspace) {
    if (view->workspace == workspace) {
        wlr_log(WLR_DEBUG, "Asked to shift view to workspace that view was already in, doing nothing");
        return;
    }

    char view_name[VIEW_NAME_LEN];
    viv_view_string_identifier(view, view_name, VIEW_NAME_LEN);
    wlr_log(WLR_DEBUG, "Shifting view %s to workspace with name %s", view_name, workspace->name);

    struct viv_workspace *cur_workspace = view->workspace;

    struct viv_view *next_view = NULL;
    if (wl_list_length(&cur_workspace->views) > 1) {
        struct wl_list *next_view_link = view->workspace_link.next;
        if (next_view_link == &cur_workspace->views) {
            next_view_link = next_view_link->next;
        }
        next_view = wl_container_of(next_view_link, next_view, workspace_link);
    }

    wl_list_remove(&view->workspace_link);
    wl_list_insert(&workspace->views, &view->workspace_link);

    if (next_view != NULL) {
        viv_view_focus(next_view, view->xdg_surface->surface);
    }

    cur_workspace->needs_layout = true;
    workspace->needs_layout = true;

    cur_workspace->active_view = next_view;
    if (workspace->active_view == NULL) {
        workspace->active_view = view;
    }
    view->workspace = workspace;
}

struct viv_view *viv_view_next_in_workspace(struct viv_view *view) {
    struct viv_workspace *workspace = view->workspace;

    struct wl_list *next_link = viv_wl_list_next_ignoring_root(&view->workspace_link, &workspace->views);

    struct viv_view *next_view = wl_container_of(next_link, next_view, workspace_link);

    return next_view;
}

struct viv_view *viv_view_prev_in_workspace(struct viv_view *view) {
    struct viv_workspace *workspace = view->workspace;

    struct wl_list *prev_link = viv_wl_list_prev_ignoring_root(&view->workspace_link, &workspace->views);

    struct viv_view *prev_view = wl_container_of(prev_link, prev_view, workspace_link);

    return prev_view;
}

void viv_view_request_close(struct viv_view *view) {
    wlr_xdg_toplevel_send_close(view->xdg_surface);
}

void viv_view_string_identifier(struct viv_view *view, char *buffer, size_t len) {
    snprintf(buffer, len, "<%s:%s>",
             view->xdg_surface->toplevel->app_id,
             view->xdg_surface->toplevel->title);
}


bool viv_view_oversized(struct viv_view *view) {
    struct wlr_box actual_geometry = { 0 };
    struct wlr_box target_geometry = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height
    };
    wlr_xdg_surface_get_geometry(view->xdg_surface, &actual_geometry);

    int leeway_px = 00;

    bool surface_exceeds_bounds = ((actual_geometry.width > (target_geometry.width + leeway_px)) ||
                                   (actual_geometry.height > (target_geometry.height + leeway_px)));

    return surface_exceeds_bounds;
}

void viv_view_make_active(struct viv_view *view) {
    view->workspace->active_view = view;
    viv_view_focus(view, view->xdg_surface->surface);
}

void viv_view_set_size(struct viv_view *view, uint32_t width, uint32_t height) {
    ASSERT(view->implementation->set_size != NULL);
    view->implementation->set_size(view, width, height);
}

void viv_view_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    ASSERT(view->implementation->get_geometry != NULL);
    view->implementation->get_geometry(view, geo_box);
}
