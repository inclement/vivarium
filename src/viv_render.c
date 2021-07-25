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
#include "viv_server.h"
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
    int sx;
    int sy;
    pixman_region32_t *damage;
    pixman_region32_t *surface_bounds;  // the actual bounds on the surface outside which it cannot draw
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
    /* This function is called for every surface that needs to be rendered. */
    struct viv_render_data *rdata = data;

    sx += rdata->sx;
    sy += rdata->sy;

    struct viv_view *view = rdata->view;
    struct wlr_output *output = rdata->output;
    struct wlr_renderer *renderer = rdata->renderer;
    pixman_region32_t *damage = rdata->damage;

    // Generate a damaged area worth drawing from the intersection of the supplied surface
    // bounds (if any) and the damaged region
    pixman_region32_t applied_surface_bounds;
    pixman_region32_init(&applied_surface_bounds);
    pixman_region32_union(&applied_surface_bounds, &applied_surface_bounds, damage);
    if (rdata->surface_bounds) {
        pixman_region32_intersect(&applied_surface_bounds, &applied_surface_bounds, rdata->surface_bounds);
    }

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

    if (pixman_region32_not_empty(&applied_surface_bounds)) {
        int num_rects;
        pixman_box32_t *rects = pixman_region32_rectangles(&applied_surface_bounds, &num_rects);
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
    }

    /* This lets the client know that we've displayed that frame and it can
     * prepare another one now if it likes. */
    // TODO: Should be wlr_presentation_surface_sampled_on_output?
    wlr_surface_send_frame_done(surface, rdata->when);

    pixman_region32_fini(&applied_surface_bounds);
}

static void popup_render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
    // TODO: wlroots 0.13 probably iterates over each popup surface autoamtically
    struct viv_render_data *rdata = data;
    rdata->sx = sx;
    rdata->sy = sy;
    wlr_surface_for_each_surface(surface, render_surface, rdata);
}

static void render_rect(struct wlr_box *box, struct viv_output *output, pixman_region32_t *damage, float colour[static 4]) {
    struct wlr_output *wlr_output = output->wlr_output;
    struct wlr_renderer *renderer = output->server->renderer;

    // TODO: Need to translate box to output coords - -= output->lx etc.

    pixman_region32_t box_region_damage;
    pixman_region32_init(&box_region_damage);
    pixman_region32_union_rect(&box_region_damage, &box_region_damage, box->x, box->y, box->width, box->height);
    pixman_region32_intersect(&box_region_damage, &box_region_damage, damage);

    if (pixman_region32_not_empty(&box_region_damage)) {
        int num_rects;
        pixman_box32_t *rects = pixman_region32_rectangles(&box_region_damage, &num_rects);
        for (int i = 0; i < num_rects; i++) {
            pixman_box32_t rect = rects[i];
            struct wlr_box rect_box = {
                .x = rect.x1,
                .y = rect.y1,
                .width = rect.x2 - rect.x1,
                .height = rect.y2 - rect.y1,
            };
            wlr_renderer_scissor(renderer, &rect_box);
            wlr_render_rect(renderer, box, colour, wlr_output->transform_matrix);
        }
    }

    pixman_region32_fini(&box_region_damage);
}

/// Fills the screen around the fullscreen view with black
static void render_fullscreen_fill(struct viv_view *view, struct viv_output *output, pixman_region32_t *output_damage) {
    float black[] = {0, 0, 0, 1};
    struct wlr_box box;

    // top
    box.x = 0;
    box.y = 0;
    box.width = output->wlr_output->width;
    box.height = view->target_box.y;
    if (box.width > 0 && box.height > 0) {
        render_rect(&box, output, output_damage, black);
    }

    // left
    box.x = 0;
    box.y = view->target_box.y;
    box.width = view->target_box.x;
    box.height = view->target_box.height;
    if (box.width > 0 && box.height > 0) {
        render_rect(&box, output, output_damage, black);
    }

    // right
    box.x = view->target_box.x + view->target_box.width;
    box.y = view->target_box.y;
    box.width = output->wlr_output->width - box.x;
    box.height = view->target_box.height;
    if (box.width > 0 && box.height > 0) {
        render_rect(&box, output, output_damage, black);
    }

    // bottom
    box.x = 0;
    box.y = view->target_box.y + view->target_box.height;
    box.width = output->wlr_output->width;
    box.height = output->wlr_output->height - box.y;
    if (box.width > 0 && box.height > 0) {
        render_rect(&box, output, output_damage, black);
    }
}

/// Render the given view's borders, on the given output. The border will be the active
/// colour if is_active is true, or otherwise the inactive colour.
static void render_borders(struct viv_view *view, struct viv_output *output, pixman_region32_t *output_damage, bool is_active) {
    struct viv_server *server = output->server;
    int gap_width = server->config->gap_width;

    double x = 0, y = 0;
    wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &x, &y);
    x += view->target_box.x + gap_width;
    y += view->target_box.y + gap_width;
    int width = MAX(1, view->target_box.width - 2 * gap_width);
    int height = MAX(1, view->target_box.height - 2 * gap_width);
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
    render_rect(&box, output, output_damage, colour);

    // top
    box.x = x;
    box.y = y + height - line_width;
    box.width = width;
    box.height = line_width;
    render_rect(&box, output, output_damage, colour);

    // left
    box.x = x;
    box.y = y;
    box.width = line_width;
    box.height = height;
    render_rect(&box, output, output_damage, colour);

    // right
    box.x = x + width - line_width;
    box.y = y;
    box.width = line_width;
    box.height = height;
    render_rect(&box, output, output_damage, colour);

}

static void viv_render_xdg_view(struct wlr_renderer *renderer, struct viv_view *view, struct viv_output *output, pixman_region32_t *damage) {
    if (!view->mapped) {
        // Unmapped views don't need any further rendering
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Note this renders both the toplevel and any popups
    struct wlr_box *target_geometry = &view->target_box;

    // Update floating view sizes, as the client has control over it
    if (view->is_floating || (view->workspace->fullscreen_view == view)) {
        viv_view_match_target_box_with_surface_geometry(view);
        // TODO: recenter fullscreen?
    }

    viv_output_layout_coords_box_to_output_coords(output, target_geometry);

    pixman_region32_t surface_bounds;
    pixman_region32_init(&surface_bounds);
    pixman_region32_copy(&surface_bounds, damage);

    bool apply_surface_bounds = (output->server->config->damage_tracking_mode == VIV_DAMAGE_TRACKING_FULL);
    if (apply_surface_bounds) {
        pixman_region32_intersect_rect(&surface_bounds, &surface_bounds, target_geometry->x, target_geometry->y, target_geometry->width, target_geometry->height);
    }

    struct viv_render_data rdata = {
        .output = output->wlr_output,
        .view = view,
        .renderer = renderer,
        .when = &now,
        .limit_render_count = true,
        .sx = 0,
        .sy = 0,
        .damage = damage,
        .surface_bounds = apply_surface_bounds ? &surface_bounds : NULL,
    };

    // Render only the main surfaces (not popups)
    wlr_surface_for_each_surface(view->xdg_surface->surface, render_surface, &rdata);

    // Then render the main surface's borders
    struct viv_seat *seat = viv_server_get_default_seat(view->server);
    bool is_grabbed = ((seat->cursor_mode != VIV_CURSOR_PASSTHROUGH) &&
                       viv_server_any_seat_grabs(view->server, view));
    bool is_active_on_current_output = ((output == output->server->active_output) &
                                        (view == view->workspace->active_view));
    bool is_active = is_grabbed || is_active_on_current_output;
    if (view->workspace->fullscreen_view == view) {
        render_fullscreen_fill(view, output, damage);
    } else if ((view->is_floating || !view->workspace->active_layout->no_borders)) {
        render_borders(view, output, damage, is_active);
    }

    // Then render any popups
    rdata.limit_render_count = false;
    rdata.surface_bounds = NULL;  // popups can exceed the primary surface region
    wlr_xdg_surface_for_each_popup_surface(view->xdg_surface, popup_render_surface, &rdata);

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

    pixman_region32_fini(&surface_bounds);
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

    struct wlr_box *target_geometry = &view->target_box;

    pixman_region32_t surface_bounds;
    pixman_region32_init(&surface_bounds);
    pixman_region32_union_rect(&surface_bounds, &surface_bounds,
                               target_geometry->x, target_geometry->y, target_geometry->width, target_geometry->height);

    struct viv_render_data rdata = {
        .output = output->wlr_output,
        .view = view,
        .renderer = renderer,
        .when = &now,
        .limit_render_count = false,
        .sx = 0,
        .sy = 0,
        .damage = damage,
        .surface_bounds = &surface_bounds,
    };

    wlr_surface_for_each_surface(viv_view_get_toplevel_surface(view), render_surface, &rdata);

    // Then render the main surface's borders
    struct viv_seat *seat = viv_server_get_default_seat(view->server);
    bool is_grabbed = ((seat->cursor_mode != VIV_CURSOR_PASSTHROUGH) &&
                       viv_server_any_seat_grabs(view->server, view));
    bool is_active_on_current_output = ((output == output->server->active_output) &
                                        (view == view->workspace->active_view));
    bool is_active = is_grabbed || is_active_on_current_output;
    if (view->workspace->fullscreen_view == view) {
        render_fullscreen_fill(view, output, damage);
    } else if (!view->is_static &&
        (view->is_floating || !view->workspace->active_layout->no_borders)) {
        render_borders(view, output, damage, is_active);
    }

    pixman_region32_fini(&surface_bounds);

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

    pixman_region32_t surface_bounds;
    pixman_region32_init(&surface_bounds);
    pixman_region32_union_rect(&surface_bounds, &surface_bounds,
                               layer_view->x, layer_view->y,
                               layer_view->layer_surface->current.actual_width,
                               layer_view->layer_surface->current.actual_height);

    struct viv_view view = {.x = layer_view->x, .y = layer_view->y, .server = output->server};
    struct viv_render_data rdata = {
        .output = output->wlr_output,
        .view = &view,
        .renderer = renderer,
        .when = &now,
        .limit_render_count = false,
        .damage = damage,
        .surface_bounds = &surface_bounds,
    };

    wlr_layer_surface_v1_for_each_surface(layer_view->layer_surface, render_surface, &rdata);

    rdata.surface_bounds = NULL;  // surface bounds don't apply to popups
    wlr_layer_surface_v1_for_each_popup_surface(layer_view->layer_surface, render_surface, &rdata);

    pixman_region32_fini(&surface_bounds);
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
    if (!attach_render_success) {
        return;
    }
    if (!needs_frame) {
        wlr_output_rollback(output->wlr_output);
        return;
    }

    if (output->server->config->damage_tracking_mode == VIV_DAMAGE_TRACKING_FRAME) {
        // If in "draw whole frame" mode, damage the full output to ensure it gets drawn
        int width, height;
        wlr_output_transformed_resolution(output->wlr_output, &width, &height);
        pixman_region32_union_rect(&damage, &damage, 0, 0, width, height);
    }

    /* The "effective" resolution can change if you rotate your outputs. */
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    /* Begin the renderer (calls glViewport and some other GL sanity checks) */
    wlr_renderer_begin(renderer, width, height);

    if (output->server->config->debug_mark_undamaged_regions) {
        // Clear the output with a solid colour, so that it is easy to
        // see what rendering has taken place this frame.
        wlr_renderer_clear(renderer, (float[]){0.95, 0.2, 0.2, 1.0});
    }

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

    if (output->current_workspace->fullscreen_view) {
        viv_render_view(renderer, output->current_workspace->fullscreen_view, output, &damage);
    } else {
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
    }

    // Overlays on top of fullscreen views
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
    output->frame_draw_count++;
    if (output->server->config->debug_mark_frame_draws) {
        uint8_t draw_stage = output->frame_draw_count % 3;
        // Note 3 colours are used so that things that take 2 frames very quickly can be seen
        float output_marker_colour[4] = {0.0, 0.0, 0.0, 1.0};
        output_marker_colour[draw_stage] = 1.0;
        struct wlr_box output_marker_box = {
            .x = 30, .y = 0, .width = 10, .height = 10
        };
        wlr_render_rect(renderer ,&output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
    }
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

    pixman_region32_fini(&damage);

    // Swap the buffers
    wlr_output_commit(output->wlr_output);

    struct viv_server *server = output->server;
    if (server->config->damage_tracking_mode == VIV_DAMAGE_TRACKING_NONE) {
        // Damage the full output so that it will be drawn again next frame
        viv_output_damage(output);
    }

    static int count = 0;
    count++;
    if (count % 200 == 0) {
        struct viv_output *out;
        wl_list_for_each(out, &server->outputs, link) {
            wlr_log(WLR_INFO, "Output name \"%s\" description \"%s\" make \"%s\" model \"%s\" serial \"%s\"",
                    out->wlr_output->name,
                    out->wlr_output->description ? out->wlr_output->description : "",
                    out->wlr_output->make,
                    out->wlr_output->model,
                    out->wlr_output->serial);
        }
    }
}
