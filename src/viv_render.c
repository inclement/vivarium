#include <time.h>

#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_compositor.h>

#include "viv_types.h"
#include "viv_view.h"

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
// TODO: should this be internal to viv_render.c?
struct viv_render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct viv_view *view;
	struct timespec *when;
    bool limit_render_count;
    uint32_t max_surfaces_to_render;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct viv_render_data *rdata = data;

    if (rdata->limit_render_count && !rdata->max_surfaces_to_render) {
        // We already rendered as many surfaces as allowed in this recursive pass
        return;
    }

	struct viv_view *view = rdata->view;
	struct wlr_output *output = rdata->output;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		return;
	}

    // Translate to output-local coordinates
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			view->server->output_layout, output, &ox, &oy);
	ox += view->x + sx;
    oy += view->y + sy;

    // Apply output scale factor
    // TODO: this needs more work elsewhere to actually work
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
	};

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current.transform);
	wlr_matrix_project_box(matrix, &box, transform, 0,
		output->transform_matrix);

	/* This takes our matrix, the texture, and an alpha, and performs the actual
	 * rendering on the GPU. */
	wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
	wlr_surface_send_frame_done(surface, rdata->when);

    if (rdata->limit_render_count) {
        rdata->max_surfaces_to_render--;
    }
}


/// Render the given view's borders, on the given output. The border will be the active
/// colour if is_active is true, or otherwise the inactive colour.
static void render_borders(struct viv_view *view, struct viv_output *output, bool is_active) {
	struct wlr_renderer *renderer = output->server->renderer;

    struct viv_server *server = output->server;

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
    double x = 0, y = 0;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &x, &y);
	x += view->target_x;
    y += view->target_y;
    int width = view->target_width;
    int height = view->target_height;
    float *colour = (is_active ?
                     server->config->active_border_colour :
                     server->config->inactive_border_colour);

    int line_width = server->config->border_width;

    // TODO: account for scale factor

    struct wlr_box box;

    // bottom
    box.x = x;
    box.y = y;
    box.width = width;
    box.height = line_width;
    wlr_render_rect(renderer, &box, colour, output->wlr_output->transform_matrix);

    // top
    box.x = x;
    box.y = y + height - line_width;
    box.width = width;
    box.height = line_width;
    wlr_render_rect(renderer, &box, colour, output->wlr_output->transform_matrix);

    // left
    box.x = x;
    box.y = y;
    box.width = line_width;
    box.height = height;
    wlr_render_rect(renderer, &box, colour, output->wlr_output->transform_matrix);

    // right
    box.x = x + width - line_width;
    box.y = y;
    box.width = line_width;
    box.height = height;
    wlr_render_rect(renderer, &box, colour, output->wlr_output->transform_matrix);

}

void viv_render_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output) {
    if (!view->mapped) {
        // Unmapped views don't need any further rendering
        return;
    }

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

    struct viv_render_data rdata = {
        .output = output->wlr_output,
        .view = view,
        .renderer = renderer,
        .when = &now,
        .limit_render_count = true,
        .max_surfaces_to_render = 1 + wl_list_length(&view->xdg_surface->surface->subsurfaces),
    };

    // Note this renders both the toplevel and any popups
    struct wlr_box actual_geometry = { 0 };
    struct wlr_box target_geometry = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height
    };
    wlr_xdg_surface_get_geometry(view->xdg_surface, &actual_geometry);

    bool surface_exceeds_bounds = viv_view_oversized(view);

    // Render only the main surface
    if (surface_exceeds_bounds) {
        // Only scissor if the view's surface exceeds its bounds, to save a tiny bit of time

        double ox = 0, oy = 0;
        wlr_output_layout_output_coords(view->server->output_layout, output->wlr_output, &ox, &oy);
        target_geometry.x += ox;
        target_geometry.y += oy;

        wlr_renderer_scissor(renderer, &target_geometry);
    }
    wlr_surface_for_each_surface(view->xdg_surface->surface, render_surface, &rdata);
    if (surface_exceeds_bounds) {
        wlr_renderer_scissor(renderer, NULL);
    }

    // Then render the main surface's borders
    bool is_grabbed = ((output->server->cursor_mode != VIV_CURSOR_PASSTHROUGH) &&
                       (view == output->server->grab_state.view));
    bool is_active_on_current_output = ((output == output->server->active_output) &
                                        (view == view->workspace->active_view));
    bool is_active = is_grabbed || is_active_on_current_output;
    render_borders(view, output, is_active);

    // Then render any popups
    rdata.limit_render_count = false;
    wlr_xdg_surface_for_each_popup(view->xdg_surface, render_surface, &rdata);

}

void viv_render_layer_view(struct wlr_renderer *renderer, struct viv_layer_view *layer_view, struct viv_output *output) {
    if (!layer_view->mapped) {
        // Unmapped layer views don't need drawing
        return;
    }

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

    struct viv_view view = {.x = layer_view->x, .y = layer_view->y, .server = output->server};

    struct viv_render_data rdata = {
        .output = output->wlr_output,
        .view = &view,
        .renderer = renderer,
        .when = &now,
        .limit_render_count = false,
    };

    wlr_layer_surface_v1_for_each_surface(layer_view->layer_surface, render_surface, &rdata);
    wlr_layer_surface_v1_for_each_popup(layer_view->layer_surface, render_surface, &rdata);
}

static bool viv_layer_is(struct viv_layer_view *layer_view, enum zwlr_layer_shell_v1_layer layer) {
    if (layer_view->layer_surface->current.layer == layer) {
        return true;
    }
    return false;
}

void viv_render_output(struct wlr_renderer *renderer, struct viv_output *output) {
	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		return;
	}
	/* The "effective" resolution can change if you rotate your outputs. */
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	/* Begin the renderer (calls glViewport and some other GL sanity checks) */
	wlr_renderer_begin(renderer, width, height);

	wlr_renderer_clear(renderer, output->server->config->clear_colour);

	struct viv_view *view;

    struct viv_layer_view *layer_view;

    // First render layer-protocol surfaces in the background or bottom layers
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)) {
            viv_render_layer_view(renderer, layer_view, output);
        }
    }
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)) {
            viv_render_layer_view(renderer, layer_view, output);
        }
    }

    // Begin rendering actual views: first render tiled windows
	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (view->is_floating || (view == output->current_workspace->active_view)) {
            continue;
        }
        viv_render_view(renderer, view, output);
	}

    // Then render the active view if necessary
    if ((output->current_workspace->active_view != NULL) &&
        (output->current_workspace->active_view->mapped) &&
        (!output->current_workspace->active_view->is_floating)) {
        viv_render_view(renderer, output->current_workspace->active_view, output);
    }

    // Render floating views that may be overhanging other workspaces
    struct viv_output *other_output;
    wl_list_for_each(other_output, &output->server->outputs, link) {
        if (other_output == output) {
            continue;
        }
        struct viv_workspace *other_workspace = other_output->current_workspace;
        wl_list_for_each(view, &other_workspace->views, workspace_link) {
            if (!view->is_floating) {
                continue;
            }
            viv_render_view(renderer, view, output);
        }
    }

    // Finally render all floating views on this output (which may include the active view)
	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (!view->is_floating) {
            continue;
        }
        viv_render_view(renderer, view, output);
	}

    // Render any layer surfaces that should go on top of views
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
            viv_render_layer_view(renderer, layer_view, output);
        }
    }
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)) {
            viv_render_layer_view(renderer, layer_view, output);
        }
    }

#ifdef DEBUG
    // Mark the currently-active output
    struct wlr_box output_marker_box = {
        .x = 0, .y = 0, .width = 10, .height = 10
    };
    float output_marker_colour[4] = {0.5, 0.5, 1, 0.5};
    if (output == output->server->active_output) {
        wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
    }
#endif

    // Have wlroots render software cursors if necessary (does nothing
    // if hardware cursors available)
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	// Conclude rendering and swap the buffers
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);
}
