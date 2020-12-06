#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_compositor.h>

#include "viv_types.h"

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct viv_render_data *rdata = data;
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
    };

    // Note this renders both the toplevel and any popups
    wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, &rdata);

    // TODO: We should actually do the following in order:
    // - render the toplevel (but only the region within the intended layout area)
    // - render the borders
    // - render any popups
    bool is_active = ((output == output->server->active_output) &
                      (view == view->workspace->active_view));

    render_borders(view, output, is_active);
}
