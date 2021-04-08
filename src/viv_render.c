#include <time.h>

#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/region.h>
#include <wlr/xwayland.h>
#include <wayland-util.h>

#include <pixman-1/pixman.h>

#include "viv_types.h"
#include "viv_output.h"
#include "viv_view.h"

#define MAX(A, B) (A > B ? A : B)

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
    int sx;
    int sy;
    pixman_region32_t *damage;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct viv_render_data *rdata = data;

    sx += rdata->sx;
    sy += rdata->sy;

    if (rdata->limit_render_count && !rdata->max_surfaces_to_render) {
        // We already rendered as many surfaces as allowed in this recursive pass
        return;
    }

	struct viv_view *view = rdata->view;
	struct wlr_output *output = rdata->output;
    struct wlr_renderer *renderer = rdata->renderer;
    pixman_region32_t *damage = rdata->damage;

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

    int num_rects;
	pixman_box32_t *rects = pixman_region32_rectangles(damage, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        pixman_box32_t rect = rects[i];
        struct wlr_box box = {
            .x = rect.x1,
            .y = rect.y1,
            .width = rect.x2 - rect.x1,
            .height = rect.y2 - rect.y1,
        };
        int output_width, output_height;
        wlr_output_transformed_resolution(output, &output_width, &output_height);
        wlr_box_transform(&box, &box, transform, output_width, output_height);
        wlr_renderer_scissor(renderer, &box);

        /* This takes our matrix, the texture, and an alpha, and performs the actual
        * rendering on the GPU. */
        wlr_render_texture_with_matrix(rdata->renderer, texture, matrix, 1);
    }

	/* This lets the client know that we've displayed that frame and it can
	 * prepare another one now if it likes. */
    // TODO: Should be wlr_presentation_surface_sampled_on_output?
	wlr_surface_send_frame_done(surface, rdata->when);

    if (rdata->limit_render_count) {
        rdata->max_surfaces_to_render--;
    }
}

static void popup_render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
    // TODO: wlroots 0.13 probably iterates over each popup surface autoamtically
	struct viv_render_data *rdata = data;
    rdata->sx = sx;
    rdata->sy = sy;
    wlr_surface_for_each_surface(surface, render_surface, rdata);
}

static void render_rect_borders(struct wlr_renderer *renderer, struct viv_server *server, struct viv_output *output, double x, double y, int width, int height) {
    double lx = 0, ly = 0;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &lx, &ly);
    x += lx;
    y += ly;
    float colour[4] = {0.1, 0.3, 1.0, 0.0};

    int line_width = 3;

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

/// Render the given view's borders, on the given output. The border will be the active
/// colour if is_active is true, or otherwise the inactive colour.
static void render_borders(struct viv_view *view, struct viv_output *output, bool is_active) {
	struct wlr_renderer *renderer = output->server->renderer;

    struct viv_server *server = output->server;
    int gap_width = server->config->gap_width;

    double x = 0, y = 0;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &x, &y);
	x += view->target_x + gap_width;
    y += view->target_y + gap_width;
    int width = MAX(1, view->target_width - 2 * gap_width);
    int height = MAX(1, view->target_height - 2 * gap_width);
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

static void viv_render_xdg_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output, pixman_region32_t *damage) {
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
        .sx = 0,
        .sy = 0,
        .damage = damage,
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

    // Always clear the scissoring so that we can draw borders anywhere
    wlr_renderer_scissor(renderer, NULL);

    // Then render the main surface's borders
    bool is_grabbed = ((output->server->cursor_mode != VIV_CURSOR_PASSTHROUGH) &&
                       (view == output->server->grab_state.view));
    bool is_active_on_current_output = ((output == output->server->active_output) &
                                        (view == view->workspace->active_view));
    bool is_active = is_grabbed || is_active_on_current_output;
    if (view->is_floating || !view->workspace->active_layout->no_borders) {
        render_borders(view, output, is_active);
    }

    // Then render any popups
    rdata.limit_render_count = false;
    wlr_xdg_surface_for_each_popup(view->xdg_surface, popup_render_surface, &rdata);

#ifdef DEBUG
    if (output->server->config->debug_mark_views_by_shell) {
        // Mark this as an xdg view
        struct wlr_box output_marker_box = {
            .x = view->x, .y = view->y, .width = 10, .height = 10
        };
        float output_marker_colour[4] = {0, 1, 0, 0.5};
        if (output == output->server->active_output) {
            wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
        }
    }
#endif
}

#ifdef XWAYLAND
static void viv_render_xwayland_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output, pixman_region32_t *damage) {
    UNUSED(damage);
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
        .limit_render_count = false,
        .sx = 0,
        .sy = 0,
        .damage = damage,
    };

    struct wlr_box target_geometry = {
        .x = view->target_x,
        .y = view->target_y,
        .width = view->target_width,
        .height = view->target_height
    };

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
    wlr_surface_for_each_surface(viv_view_get_toplevel_surface(view), render_surface, &rdata);
    if (surface_exceeds_bounds) {
        wlr_renderer_scissor(renderer, NULL);
    }

    // Then render the main surface's borders
    bool is_grabbed = ((output->server->cursor_mode != VIV_CURSOR_PASSTHROUGH) &&
                       (view == output->server->grab_state.view));
    bool is_active_on_current_output = ((output == output->server->active_output) &
                                        (view == view->workspace->active_view));
    bool is_active = is_grabbed || is_active_on_current_output;
    if (!view->is_static &&
        (view->is_floating || !view->workspace->active_layout->no_borders)) {
        render_borders(view, output, is_active);
    }

#ifdef DEBUG
    if (output->server->config->debug_mark_views_by_shell) {
        // Mark this as an xwayland view
        struct wlr_box output_marker_box = {
            .x = view->x, .y = view->y, .width = 10, .height = 10
        };
        float output_marker_colour[4] = {1, 0, 0, 0.5};
        if (output == output->server->active_output) {
            wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
        }
    }
#endif  // DEBUG
}
#endif  // XWAYLAND

void viv_render_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output, pixman_region32_t *damage) {
    switch (view->type) {
    case VIV_VIEW_TYPE_XDG_SHELL:
        viv_render_xdg_view(renderer, view, output, damage);
        break;
#ifdef XWAYLAND
    case VIV_VIEW_TYPE_XWAYLAND:
        viv_render_xwayland_view(renderer, view, output, damage);
        break;
#endif
    default:
        UNREACHABLE();
    }
}


void viv_render_layer_view(struct wlr_renderer *renderer, struct viv_layer_view *layer_view, struct viv_output *output, pixman_region32_t *damage) {
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
        .damage = damage,
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
    pixman_region32_t damage;
    bool needs_frame;
    pixman_region32_init(&damage);
    bool attach_render_success = wlr_output_damage_attach_render(output->damage, &needs_frame, &damage);
    wlr_log(WLR_INFO, "Return %d, needs frame: %d", attach_render_success, needs_frame);
	if (!attach_render_success) {
		return;
	}
    if (!needs_frame) {
        wlr_output_rollback(output->wlr_output);
        return;
    }
	/* The "effective" resolution can change if you rotate your outputs. */
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	/* Begin the renderer (calls glViewport and some other GL sanity checks) */
	wlr_renderer_begin(renderer, width, height);

    /* wlr_renderer_clear(renderer, (float[]){1.0, 0.0, 0.0, 1.0}); */

	/* wlr_renderer_clear(renderer, (float[]){0.1, 0.1, 0.1, 1.0}); */
    int num_rects;
    pixman_box32_t *rects = pixman_region32_rectangles(&damage, &num_rects);
    for (int i = 0; i < num_rects; i++) {
        pixman_box32_t rect = rects[i];
        struct wlr_box box = {
            .x = rect.x1,
            .y = rect.y1,
            .width = rect.x2 - rect.x1,
            .height = rect.y2 - rect.y1,
        };
        wlr_renderer_scissor(renderer, &box);
        wlr_renderer_clear(renderer, output->server->config->clear_colour);
    }

	struct viv_view *view;

    struct viv_layer_view *layer_view;

    // First render layer-protocol surfaces in the background or bottom layers
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)) {
            viv_render_layer_view(renderer, layer_view, output, &damage);
        }
    }
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)) {
            viv_render_layer_view(renderer, layer_view, output, &damage);
        }
    }

    // Begin rendering actual views: first render tiled windows
	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (view->is_floating || (view == output->current_workspace->active_view)) {
            continue;
        }
        viv_render_view(renderer, view, output, &damage);
	}

    // Then render the active view if necessary
    if ((output->current_workspace->active_view != NULL) &&
        (output->current_workspace->active_view->mapped) &&
        (!output->current_workspace->active_view->is_floating)) {
        viv_render_view(renderer, output->current_workspace->active_view, output, &damage);
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
            viv_render_view(renderer, view, output, &damage);
        }
    }

    // Finally render all floating views on this output (which may include the active view)
	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (!view->is_floating) {
            continue;
        }
        viv_render_view(renderer, view, output, &damage);
	}

    // Render any layer surfaces that should go on top of views
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
            viv_render_layer_view(renderer, layer_view, output, &damage);
        }
    }
    wl_list_for_each_reverse(layer_view, &output->layer_views, output_link) {
        if (viv_layer_is(layer_view, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)) {
            viv_render_layer_view(renderer, layer_view, output, &damage);
        }
    }

    wlr_renderer_scissor(renderer, NULL);

#ifdef DEBUG
    // Mark the currently-active output
    if (output->server->config->debug_mark_active_output) {
        struct wlr_box output_marker_box = {
            .x = 0, .y = 0, .width = 10, .height = 10
        };
        float output_marker_colour[4] = {0.5, 0.5, 1, 0.5};
        if (output == output->server->active_output) {
            wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
        }
    }

    // Indicate when a frame is drawn
    static bool even = true;
    even = !even;
    if (even) {
        struct wlr_box output_marker_box = {
            .x = 30, .y = 0, .width = 10, .height = 10
        };
        float output_marker_colour[4] = {1.0, 0.0, 0.0, 1.0};
        wlr_render_rect(renderer ,&output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
    } else {
        struct wlr_box output_marker_box = {
            .x = 30, .y = 0, .width = 10, .height = 10
        };
        float output_marker_colour[4] = {0.0, 1.0, 0.0, 1.0};
        wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
    }

    /* // Mark damaged regions */
    /* float damage_colour[] = {1.0, 0.0, 1.0, 0.05}; */
    /* for (int i = 0; i < num_rects; i++) { */
    /*     pixman_box32_t rect = rects[i]; */
    /*     struct wlr_box box = { */
    /*         .x = rect.x1, */
    /*         .y = rect.y1, */
    /*         .width = rect.x2 - rect.x1, */
    /*         .height = rect.y2 - rect.y1, */
    /*     }; */
    /*     wlr_render_rect(renderer, &box, damage_colour, output->wlr_output->transform_matrix); */
    /* } */

    /* // Mark damaged regions */
    /* for (int i = 0; i < num_rects; i++) { */
    /*     pixman_box32_t rect = rects[i]; */
    /*     struct wlr_box box = { */
    /*         .x = rect.x1, */
    /*         .y = rect.y1, */
    /*         .width = rect.x2 - rect.x1, */
    /*         .height = rect.y2 - rect.y1, */
    /*     }; */
    /*     render_rect_borders(renderer, output->server, output, box.x, box.y, box.width, box.height); */
    /* } */
    UNUSED(render_rect_borders);
#endif

    // Have wlroots render software cursors if necessary (does nothing
    // if hardware cursors available)
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	// Conclude rendering
	wlr_renderer_end(renderer);

    // Calculate the frame damage before swapping the buffers
	pixman_region32_t frame_damage;
	pixman_region32_init(&frame_damage);

    // Fill in frame damage
    struct wlr_output *wlr_output = output->wlr_output;
	int fwidth, fheight;
	wlr_output_transformed_resolution(wlr_output, &fwidth, &fheight);
	enum wl_output_transform transform = wlr_output_transform_invert(wlr_output->transform);
    wlr_region_transform(&frame_damage, &output->damage->current, transform, fwidth, fheight);

    wlr_output_set_damage(output->wlr_output, &frame_damage);
	pixman_region32_fini(&frame_damage);

    // Swap the buffers
	wlr_output_commit(output->wlr_output);

#ifdef DEBUG
    struct viv_server *server = output->server;
    if (server->config->debug_no_damage_tracking) {
        viv_output_damage(output);
    }
#endif
}
