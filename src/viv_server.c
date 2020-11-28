#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "viv_types.h"
#include "viv_cursor.h"
#include "viv_server.h"
#include "viv_workspace.h"
#include "viv_layout.h"
#include "viv_output.h"
#include "viv_view.h"

#include "viv_config.h"
#include "viv_config_support.h"

/** Test if any surfaces of the given view are at the given layout coordinates, including
    nested surfaces (e.g. popup windows, tooltips).  If so, return the surface data.
    @param view Pointer to the view to test
    @param lx Position to test in layout coordinates
    @param ly Position to test in layout coordinates
    @param surface wlr_surface via which to return if found
    @param sx Surface coordinate to return (relative to surface top left corner)
    @param sy Surface coordinate to return (relative to surface top left corner)
    @returns true if a surface was found, else false
*/
static bool is_view_at(struct viv_view *view,
		double lx, double ly, struct wlr_surface **surface,
		double *sx, double *sy) {
	double view_sx = lx - view->x;
	double view_sy = ly - view->y;

	double _sx, _sy;
	struct wlr_surface *_surface = NULL;
	_surface = wlr_xdg_surface_surface_at(
			view->xdg_surface, view_sx, view_sy, &_sx, &_sy);

	if (_surface != NULL) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return true;
	}

	return false;
}

/** Test if any views being handled by the compositor are present at
    the given layout coordinates lx,ly. This is at server level
    because it checks for against all views in the server.
 */
struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {

    // Note: this all relies on the list of surfaces held by a workspace being ordered
    // from top to bottom

    // TODO we should check floating views from other outputs as well

    // TODO should this be in an output_layouts type helper file?

	struct viv_view *view;
    // Try floating views first
	wl_list_for_each(view, &server->active_output->current_workspace->views, workspace_link) {
        if (!view->is_floating) {
            continue;
        }
		if (is_view_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}
    // No floating view found => try other views
	wl_list_for_each(view, &server->active_output->current_workspace->views, workspace_link) {
		if (is_view_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}
	return NULL;
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

    // Pass the movement along (i.e. allow the cursor to actually move)
	wlr_cursor_move(server->cursor, event->device,
			event->delta_x, event->delta_y);

    wlr_log(WLR_DEBUG, "Cursor at %f, %f", server->cursor->x, server->cursor->y);
    // Do our own processing of the motion if necessary
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
    wlr_log(WLR_DEBUG, "Cursor at %f, %f", server->cursor->x, server->cursor->y);
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

static void begin_interactive(struct viv_view *view,
		enum viv_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct viv_server *server = view->server;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (view->xdg_surface->surface != focused_surface) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grab_state.view = view;
	server->cursor_mode = mode;

    // Any view can be interacted with, but this automatically pulls it out of tiling
    viv_view_ensure_floating(view);

	if (mode == VIV_CURSOR_MOVE) {
		server->grab_state.x = server->cursor->x - view->x;
		server->grab_state.y = server->cursor->y - view->y;
	} else {
		struct wlr_box geo_box;
		wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);

		double border_x = (view->x + geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = (view->y + geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_state.x = server->cursor->x - border_x;
		server->grab_state.y = server->cursor->y - border_y;

		server->grab_state.geobox = geo_box;
		server->grab_state.geobox.x += view->x;
		server->grab_state.geobox.y += view->y;

		server->grab_state.resize_edges = edges;
	}
}

static bool global_meta_held(struct viv_server *server) {
    struct viv_keyboard *keyboard;
    wl_list_for_each(keyboard, &server->keyboards, link) {
        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
        if (modifiers & server->config->global_meta_key) {
            return true;
        }
    }
    return false;
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;
	/* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
	double sx, sy;
	struct wlr_surface *surface;
	struct viv_view *view = viv_server_view_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize mode. */
		server->cursor_mode = VIV_CURSOR_PASSTHROUGH;
	} else {
		/* Focus that client if the button was pressed */
		viv_view_focus(view, surface);
        if (global_meta_held(server)) {
            viv_view_bring_to_front(view);
            if (event->state == WLR_BUTTON_PRESSED) {
                if (event->button == VIV_LEFT_BUTTON) {
                    begin_interactive(view, VIV_CURSOR_MOVE, 0);
                } else if (event->button == VIV_RIGHT_BUTTON) {
                    begin_interactive(view, VIV_CURSOR_RESIZE, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
                }
            }
        }
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

/* Used to move all of the data necessary to render a surface from the top-level
 * frame handler to the per-surface render function. */
struct render_data {
	struct wlr_output *output;
	struct wlr_renderer *renderer;
	struct viv_view *view;
	struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	/* This function is called for every surface that needs to be rendered. */
	struct render_data *rdata = data;
	struct viv_view *view = rdata->view;
	struct wlr_output *output = rdata->output;

	/* We first obtain a wlr_texture, which is a GPU resource. wlroots
	 * automatically handles negotiating these with the client. The underlying
	 * resource could be an opaque handle passed from the client, or the client
	 * could have sent a pixel buffer which we copied to the GPU, or a few other
	 * means. You don't have to worry about this, wlroots takes care of it. */
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (texture == NULL) {
		return;
	}

	/* The view has a position in layout coordinates. If you have two displays,
	 * one next to the other, both 1080p, a view on the rightmost display might
	 * have layout coordinates of 2000,100. We need to translate that to
	 * output-local coordinates, or (2000 - 1920). */
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(
			view->server->output_layout, output, &ox, &oy);
	ox += view->x + sx, oy += view->y + sy;
    UNUSED(ox); UNUSED(oy); UNUSED(sx); UNUSED(sy);

	/* We also have to apply the scale factor for HiDPI outputs. This is only
	 * part of the puzzle, vivarium does not fully support HiDPI. */
	struct wlr_box box = {
		.x = ox * output->scale,
		.y = oy * output->scale,
		.width = surface->current.width * output->scale,
		.height = surface->current.height * output->scale,
	};

	/*
	 * Those familiar with OpenGL are also familiar with the role of matricies
	 * in graphics programming. We need to prepare a matrix to render the view
	 * with. wlr_matrix_project_box is a helper which takes a box with a desired
	 * x, y coordinates, width and height, and an output geometry, then
	 * prepares an orthographic projection and multiplies the necessary
	 * transforms to produce a model-view-projection matrix.
	 *
	 * Naturally you can do this any way you like, for example to make a 3D
	 * compositor.
	 */
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

static void render_view(struct viv_view *view, struct render_data *rdata) {
    /* This calls our render_surface function for each surface among the
        * xdg_surface's toplevel and popups. */
    wlr_xdg_surface_for_each_surface(view->xdg_surface, render_surface, rdata);
}

static void render_borders(struct viv_view *view, bool is_active) {
    struct viv_output *output = view->workspace->output;
	struct wlr_renderer *renderer = output->server->renderer;

    struct viv_server *server = output->server;

    struct wlr_box geo_box;
    wlr_xdg_surface_get_geometry(view->xdg_surface, &geo_box);
    int x = view->target_x;
    int y = view->target_y;
    int width = view->target_width;
    int height = view->target_height;
    float *colour = (is_active ?
                     server->config->active_border_colour :
                     server->config->inactive_border_colour);

    int line_width = server->config->border_width;

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

static void output_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct viv_output *output = wl_container_of(listener, output, frame);
	struct wlr_renderer *renderer = output->server->renderer;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* wlr_output_attach_render makes the OpenGL context current. */
	if (!wlr_output_attach_render(output->wlr_output, NULL)) {
		return;
	}
	/* The "effective" resolution can change if you rotate your outputs. */
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	/* Begin the renderer (calls glViewport and some other GL sanity checks) */
	wlr_renderer_begin(renderer, width, height);

	wlr_renderer_clear(renderer, output->server->config->clear_colour);

	/* Each subsequent window we render is rendered on top of the last. Because
	 * our view list is ordered front-to-back, we iterate over it backwards. */
    // First render tiled windows, then render floating windows on top (but preserving order)
	struct viv_view *view;
	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (view->is_floating) {
            continue;
        }
		if (!view->mapped) {
			// An unmapped view should not be rendered.
			continue;
		}
        if (view == output->current_workspace->active_view) {
            // The active view always gets rendered on top
            continue;
        }
		struct render_data rdata = {
			.output = output->wlr_output,
			.view = view,
			.renderer = renderer,
			.when = &now,
		};
        render_view(view, &rdata);
        render_borders(view, false);
	}

    if (output->current_workspace->active_view != NULL) {
        if (output->current_workspace->active_view->mapped &
            !output->current_workspace->active_view->is_floating) {
            // Render the active view
            struct render_data rdata = {
                .output = output->wlr_output,
                .view = output->current_workspace->active_view,
                .renderer = renderer,
                .when = &now,
            };
            render_view(output->current_workspace->active_view, &rdata);
            bool output_is_active = (output == output->server->active_output);
            render_borders(output->current_workspace->active_view, output_is_active);
        }
    }

	wl_list_for_each_reverse(view, &output->current_workspace->views, workspace_link) {
        if (!view->is_floating) {
            continue;
        }
		if (!view->mapped) {
			/* An unmapped view should not be rendered. */
			continue;
		}
		struct render_data rdata = {
			.output = output->wlr_output,
			.view = view,
			.renderer = renderer,
			.when = &now,
		};
		/* This calls our render_surface function for each surface among the
		 * xdg_surface's toplevel and popups. */
        render_view(view, &rdata);
        bool is_active_window = (output->current_workspace->active_view == view);
        render_borders(view, is_active_window);
	}

    struct wlr_box output_marker_box = {
        .x = 0, .y = 0, .width = 10, .height = 10
    };
    float output_marker_colour[4] = {0.5, 0.5, 1, 0.5};
    if (output == output->server->active_output) {
        wlr_render_rect(renderer, &output_marker_box, output_marker_colour, output->wlr_output->transform_matrix);
    }

	/* Hardware cursors are rendered by the GPU on a separate plane, and can be
	 * moved around without re-rendering what's beneath them - which is more
	 * efficient. However, not all hardware supports hardware cursors. For this
	 * reason, wlroots provides a software fallback, which we ask it to render
	 * here. wlr_cursor handles configuring hardware vs software cursors for you,
	 * and this function is a no-op when hardware cursors are in use. */
	wlr_output_render_software_cursors(output->wlr_output, NULL);

	/* Conclude rendering and swap the buffers, showing the final frame
	 * on-screen. */
	wlr_renderer_end(renderer);
	wlr_output_commit(output->wlr_output);

    // TODO this probably shouldn't be here?  For now do layout right after committing a
    // frame, to give time for clients to re-draw before the next one. There's probably a
    // better way to do this.
    struct viv_workspace *workspace = output->current_workspace;
    if (output->needs_layout | (output->current_workspace->needs_layout)) {
        workspace->active_layout->layout_function(workspace);
        output->needs_layout = false;
        output->current_workspace->needs_layout = false;
    }
}

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct viv_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
	 * before we can use the output. The mode is a tuple of (width, height,
	 * refresh rate), and each monitor supports only a specific set of modes. We
	 * just pick the monitor's preferred mode, a more sophisticated compositor
	 * would let the user configure it. */
	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	/* Allocates and configures our state for this output */
	struct viv_output *output = calloc(1, sizeof(struct viv_output));
    CHECK_ALLOCATION(output);

	output->wlr_output = wlr_output;
	output->server = server;
	/* Sets up a listener for the frame notify event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

    static size_t count = 0;
    struct viv_workspace *current_workspace = wl_container_of(server->workspaces.next, current_workspace, server_link);
    for (size_t i = 0; i < count; i++) {
        current_workspace = wl_container_of(current_workspace->server_link.next, current_workspace, server_link);
    }
    count++;
    output->current_workspace = current_workspace;
    current_workspace->output = output;
    wlr_log(WLR_INFO, "Assigning new output workspace %s", current_workspace->name);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	wlr_output_layout_add_auto(server->output_layout, wlr_output);

    server->active_output = output;
}

static void xdg_surface_map(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct viv_view *view = wl_container_of(listener, view, map);
	view->mapped = true;
	viv_view_focus(view, view->xdg_surface->surface);

    struct viv_workspace *workspace = view->workspace;
    workspace->output->needs_layout += 1;
}

static void xdg_surface_unmap(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct viv_view *view = wl_container_of(listener, view, unmap);
	view->mapped = false;

    struct viv_workspace *workspace = view->workspace;
    workspace->output->needs_layout += 1;
}

static void xdg_surface_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* Called when the surface is destroyed and should never be shown again. */
	struct viv_view *view = wl_container_of(listener, view, destroy);

    struct viv_workspace *workspace = view->workspace;

	wl_list_remove(&view->workspace_link);

    if (wl_list_length(&workspace->views) > 0) {
        struct viv_view *new_active_view = wl_container_of(workspace->views.next, new_active_view, workspace_link);
        workspace->active_view = new_active_view;
        viv_view_focus(workspace->active_view, view->xdg_surface->surface);
    }

	free(view);
}


static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provied serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct viv_view *view = wl_container_of(listener, view, request_move);
	begin_interactive(view, VIV_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct viv_view *view = wl_container_of(listener, view, request_resize);
	begin_interactive(view, VIV_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, request_maximize);
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_ERROR, "\"%s\" requested maximise, ignoring", app_id);
}

static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, request_minimize);
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_ERROR, "\"%s\" requested minimise, ignoring", app_id);
}

static void xdg_toplevel_set_title(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_view *view = wl_container_of(listener, view, set_title);
    const char *title = view->xdg_surface->toplevel->title;
    const char *app_id = view->xdg_surface->toplevel->app_id;
    wlr_log(WLR_DEBUG, "\"%s\" set title \"%s\"", app_id, title);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct viv_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	/* Allocate a viv_view for this surface */
	struct viv_view *view = calloc(1, sizeof(struct viv_view));
    CHECK_ALLOCATION(view);

    view->type = VIV_VIEW_TYPE_XDG_SHELL;
	view->server = server;
	view->xdg_surface = xdg_surface;

	/* Listen to the various events it can emit */
	view->map.notify = xdg_surface_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_surface_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_surface_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	/* cotd */
	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	view->request_minimize.notify = xdg_toplevel_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
	view->set_title.notify = xdg_toplevel_set_title;
	wl_signal_add(&toplevel->events.set_title, &view->set_title);

    /* Add it to a workspace */
    static size_t count = 0;
    struct viv_output *output = wl_container_of(server->outputs.next, output, link);
    if (count % 2 == 0) {
        output = wl_container_of(output->link.next, output, link);
    }
    count++;
    wl_list_insert(&output->current_workspace->views, &view->workspace_link);
    view->workspace = output->current_workspace;
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    UNUSED(data);
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct viv_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->device->keyboard->modifiers);
}

static bool handle_keybinding(struct viv_server *server, xkb_keysym_t sym) {
    struct viv_output *output = wl_container_of(server->outputs.next, output, link);

    struct viv_workspace *workspace = output->current_workspace;

    struct viv_keybind keybind;
    for (uint32_t i = 0; i < MAX_NUM_KEYBINDS; i++) {
        keybind = server->config->keybinds[i];

        if (keybind.key == NULL_KEY) {
            break;
        }

        if (keybind.key == sym) {
            keybind.binding(workspace, keybind.payload);
            return true;
        }
    }
    return false;
}

static void server_keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	struct viv_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct viv_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->device->keyboard->xkb_state, keycode, &syms);


	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	if ((modifiers & server->config->global_meta_key) && event->state == WLR_KEY_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
            wlr_log(WLR_DEBUG, "Key %d pressed", syms[i]);
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void server_new_keyboard(struct viv_server *server,
		struct wlr_input_device *device) {
	struct viv_keyboard *keyboard = calloc(1, sizeof(struct viv_keyboard));
    CHECK_ALLOCATION(keyboard);

	keyboard->server = server;
	keyboard->device = device;

    struct xkb_rule_names *rules = &server->config->xkb_rules;
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(device->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = server_keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(server->seat, device);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct viv_server *server,
		struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct viv_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In Viv we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(
			listener, server, request_cursor);
	/* This event is rasied by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in viv we always honor
	 */
	struct viv_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}


/// Copy each of the layouts in the input list into a wl_list, and return this new list
static void init_layouts(struct wl_list *layouts_list, struct viv_layout *layouts) {
    wl_list_init(layouts_list);

    struct viv_layout layout_definition;
    struct viv_layout *layout_instance;
    for (size_t i = 0; i < MAX_NUM_LAYOUTS; i++) {
        layout_definition = layouts[i];
        if (strcmp(layout_definition.name, "") == 0) {
            if (i == 0) {
                EXIT_WITH_MESSAGE("No layout definitions found in config");
            }
            break;
        }

        layout_instance = calloc(1, sizeof(struct viv_layout));
        CHECK_ALLOCATION(layout_instance);

        memcpy(layout_instance, &layout_definition, sizeof(struct viv_layout));
        wl_list_insert(layouts_list->prev, &layout_instance->workspace_link);
        wlr_log(WLR_DEBUG, "Initialised layout with name %s", layout_instance->name);
    }
}

/// For each workspace name, create an initialise a ::viv_workspace with the specified
/// layouts.
static void init_workspaces(struct wl_list *workspaces_list,
                                       char workspace_names[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LENGTH],
                                       struct viv_layout *layouts) {
    wlr_log(WLR_INFO, "Making workspaces");

    wl_list_init(workspaces_list);

    char *name;
    struct viv_workspace *workspace;
    for (size_t i = 0; i < MAX_NUM_WORKSPACES; i++) {
        name = workspace_names[i];
        if (!strlen(name)) {
            wlr_log(WLR_DEBUG, "No more workspace names found");
            break;
        }
        wlr_log(WLR_DEBUG, "Making workspace with name %s", name);

        // Allocate the workspace and add to the return list
        workspace = calloc(1, sizeof(struct viv_workspace));
        CHECK_ALLOCATION(workspace);

        memcpy(workspace->name, name, sizeof(char) * MAX_WORKSPACE_NAME_LENGTH);
        wl_list_init(&workspace->views);
        wl_list_insert(workspaces_list->prev, &workspace->server_link);

        // Set up layouts for the new workspace
        init_layouts(&workspace->layouts, layouts);
        struct viv_layout *active_layout = wl_container_of(workspace->layouts.next, active_layout, workspace_link);
        workspace->active_layout = active_layout;
    }
}


void viv_server_init(struct viv_server *server) {
    // Initialise logging, NULL indicates no callback so default logger is used
    wlr_log_init(WLR_DEBUG, NULL);

    // The config is declared globally for easy configuration, we just have to use it.
    server->config = &the_config;

    // Dynamically create workspaces according to the user configuration
    init_workspaces(&server->workspaces, server->config->workspaces, server->config->layouts);

    // Prepare our app's wl_display
	server->wl_display = wl_display_create();

    // Create a wlroots backend. This will automatically handle creating a suitable
    // backend for the environment, e.g. an X11 window if running under X.
	server->backend = wlr_backend_autocreate(server->wl_display, NULL);

    // Init the default wlroots GLES2 renderer
	server->renderer = wlr_backend_get_renderer(server->backend);
	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

    // Create some default wlroots interfaces:
    // Compositor to handle surface allocation
	wlr_compositor_create(server->wl_display, server->renderer);
    // Data device manager to handle the clipboard
	wlr_data_device_manager_create(server->wl_display);

    // Create an output layout, for handling the arrangement of multiple outputs
	server->output_layout = wlr_output_layout_create();

    // Init server outputs list and handling for new outputs
	wl_list_init(&server->outputs);
	server->new_output.notify = server_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

    // Set up the xdg-shell
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display);
	server->new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
			&server->new_xdg_surface);

    // Create a wlroots cursor for handling the cursor image shown on the screen
	server->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server->cursor, server->output_layout);

    // Use a wlroots xcursor manager to handle the cursor theme
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 1);

    // Set up events to be able to move our cursor in response to inputs
	server->cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);
	server->cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server->cursor->events.motion_absolute,
			&server->cursor_motion_absolute);
	server->cursor_button.notify = server_cursor_button;
	wl_signal_add(&server->cursor->events.button, &server->cursor_button);
	server->cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);
	server->cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);

    // Set up a wlroots "seat" handling the combination of user input devices
	wl_list_init(&server->keyboards);
	server->new_input.notify = server_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
	server->seat = wlr_seat_create(server->wl_display, "seat0");
	server->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor,
			&server->request_cursor);
	server->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server->seat->events.request_set_selection,
			&server->request_set_selection);

    wlr_log(WLR_INFO, "New viv_server initialised");

}
