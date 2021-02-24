#include <stdio.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "viv_view.h"

#include "viv_server.h"
#include "viv_types.h"
#include "viv_wl_list_utils.h"
#include "viv_workspace.h"

#define VIEW_NAME_LEN 100

void viv_view_bring_to_front(struct viv_view *view) {
    struct wl_list *link = &view->workspace_link;
    wl_list_remove(link);
    wl_list_insert(&view->workspace->views, link);
}

void viv_view_clear_all_focus(struct viv_server *server) {
    wlr_seat_keyboard_notify_clear_focus(server->seat);
}

void viv_view_focus(struct viv_view *view, struct wlr_surface *surface) {
	if (view == NULL) {
		return;
	}
    if (surface == NULL) {
        surface = viv_view_get_toplevel_surface(view);
    }
	struct viv_server *server = view->server;

	/* Activate the new surface */
    view->workspace->active_view = view;
    viv_surface_focus(server, surface);
    viv_view_set_activated(view, true);
}

struct wlr_surface *viv_view_get_toplevel_surface(struct viv_view *view) {
    return view->implementation->get_toplevel_surface(view);
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
    view->implementation->set_tiled(view, all_edges);
}

void viv_view_shift_to_workspace(struct viv_view *view, struct viv_workspace *workspace) {
    if (view->workspace == workspace) {
        wlr_log(WLR_DEBUG, "Asked to shift view to workspace that view was already in, doing nothing");
        return;
    }

    char view_name[VIEW_NAME_LEN];
    viv_view_get_string_identifier(view, view_name, VIEW_NAME_LEN);
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

    struct viv_view *next_view;
    if (wl_list_length(&workspace->views) > 1) {
        struct wl_list *next_link = viv_wl_list_next_ignoring_root(&view->workspace_link, &workspace->views);
        next_view = wl_container_of(next_link, next_view, workspace_link);
    } else {
        next_view = view;
    }

    return next_view;
}

struct viv_view *viv_view_prev_in_workspace(struct viv_view *view) {
    struct viv_workspace *workspace = view->workspace;

    struct viv_view *prev_view;
    if (wl_list_length(&workspace->views) > 1) {
        struct wl_list *prev_link = viv_wl_list_prev_ignoring_root(&view->workspace_link, &workspace->views);
        prev_view = wl_container_of(prev_link, prev_view, workspace_link);
    } else {
        prev_view = view;
    }

    return prev_view;
}

void viv_view_request_close(struct viv_view *view) {
    view->implementation->close(view);
}

void viv_view_get_string_identifier(struct viv_view *view, char *buffer, size_t len) {
    return view->implementation->get_string_identifier(view, buffer, len);
}


bool viv_view_oversized(struct viv_view *view) {
    return view->implementation->oversized(view);
}

void viv_view_make_active(struct viv_view *view) {
    view->workspace->active_view = view;
    viv_view_focus(view, view->xdg_surface->surface);
}

void viv_view_set_size(struct viv_view *view, uint32_t width, uint32_t height) {
    ASSERT(view->implementation->set_size != NULL);
    view->implementation->set_size(view, width, height);
}

void viv_view_set_pos(struct viv_view *view, uint32_t width, uint32_t height) {
    ASSERT(view->implementation->set_pos != NULL);
    view->implementation->set_pos(view, width, height);
}

void viv_view_get_geometry(struct viv_view *view, struct wlr_box *geo_box) {
    ASSERT(view->implementation->get_geometry != NULL);
    view->implementation->get_geometry(view, geo_box);
}

void viv_view_init(struct viv_view *view, struct viv_server *server) {
    // Check that the non-generic parts of the view have been initialised already
    ASSERT(view->type != VIV_VIEW_TYPE_UNKNOWN);

	view->server = server;
	view->mapped = false;

    // Make sure the view gets added to a workspace
    struct viv_output *output = server->active_output;
    view->workspace = output->current_workspace;

    viv_view_ensure_tiled(view);

    wl_list_init(&view->workspace_link);
    wl_list_insert(&server->unmapped_views, &view->workspace_link);
    /* wl_list_insert(&output->current_workspace->views, &view->workspace_link); */
}

void viv_view_destroy(struct viv_view *view) {
	wl_list_remove(&view->workspace_link);
	free(view);
}

void viv_view_set_activated(struct viv_view *view, bool activated) {
    view->implementation->set_activated(view, activated);
}

bool viv_view_is_at(struct viv_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy) {
    return view->implementation->is_at(view, lx, ly, surface, sx, sy);
}

void viv_view_set_target_box(struct viv_view *view, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    struct viv_workspace *workspace = view->workspace;
    struct viv_output *output = workspace->output;
    struct wlr_output_layout_output *output_layout_output = wlr_output_layout_get(output->server->output_layout, output->wlr_output);

    int gap_width = output->server->config->gap_width;

    int ox = output_layout_output->x;
    int oy = output_layout_output->y;
    if (!output->current_workspace->active_layout->ignore_excluded_regions) {
        ox += output->excluded_margin.left;
        oy += output->excluded_margin.top;
    }

    x += ox;
    y += oy;

    view->target_x = x;
    view->target_y = y;
    view->target_width = width;
    view->target_height = height;

    int border_width = output->server->config->border_width;
    if (output->current_workspace->active_layout->no_borders ||
        view->is_static) {
        border_width = 0u;
    }

    width -= 2 * border_width + 2 * gap_width;
    height -= 2 * border_width + 2 * gap_width;

    viv_view_set_pos(view, x + border_width + gap_width, y + border_width + gap_width);
    viv_view_set_size(view, width, height);
}

void viv_view_ensure_not_active_in_workspace(struct viv_view *view) {
    struct viv_workspace *workspace = view->workspace;
    if  (view == workspace->active_view) {
        struct wlr_seat *seat = view->workspace->server->seat;
        seat->keyboard_state.focused_surface = NULL;
        if (wl_list_length(&workspace->views) > 1) {
            viv_workspace_focus_next_window(workspace);
        } else {
            workspace->active_view = NULL;
        }
    }
}
