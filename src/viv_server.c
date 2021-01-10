#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <libinput.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>

#include "viv_background.h"
#include "viv_bar.h"
#include "viv_config_types.h"
#include "viv_types.h"
#include "viv_cursor.h"
#include "viv_input.h"
#include "viv_server.h"
#include "viv_workspace.h"
#include "viv_layout.h"
#include "viv_output.h"
#include "viv_render.h"
#include "viv_layer_view.h"
#include "viv_view.h"
#include "viv_xdg_shell.h"

#ifdef XWAYLAND
#include "viv_xwayland_shell.h"
#endif

#include "viv_debug_support.h"
#include "viv_config.h"
#include "viv_config_support.h"

struct viv_workspace *viv_server_retrieve_workspace_by_name(struct viv_server *server, char *name) {
    struct viv_workspace *workspace;
    wl_list_for_each(workspace, &server->workspaces, server_link) {
        if (strcmp(workspace->name, name) == 0) {
            return workspace;
        }
    }
    wlr_log(WLR_ERROR, "Could not find workspace with name %s", name);
    return NULL;
}

/** Test if any views being handled by the compositor are present the
    at given layout coordinates lx,ly. This is at server level
    because it checks for against all views in the server.
 */
struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {

    // Note: this all relies on the list of surfaces held by a workspace being ordered
    // from top to bottom

    struct viv_output *active_output = server->active_output;

	struct viv_view *view;
    // Try floating views first
	wl_list_for_each(view, &active_output->current_workspace->views, workspace_link) {
        if (!view->is_floating) {
            continue;
        }
		if (viv_view_is_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}

    // Try floating views from other workspaces (these are also rendered behind this workspace's views)
    // TODO: maybe decide on a better structure here
    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        if (output == active_output) {
            // We've already done the ctive output
            continue;
        }
        if (!view->is_floating) {
            // Non-floating views can't leave their output/workspace
            continue;
        }
		if (viv_view_is_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
    }

    // No floating view found => try other views
	wl_list_for_each(view, &server->active_output->current_workspace->views, workspace_link) {
		if (viv_view_is_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}
	return NULL;
}

/// Handle a cursor motion event
static void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_event_pointer_motion *event = data;

    // Pass the movement along (i.e. allow the cursor to actually move)
	wlr_cursor_move(server->cursor, event->device,
			event->delta_x, event->delta_y);

    // Do our own processing of the motion if necessary
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

void viv_surface_focus(struct viv_server *server, struct wlr_surface *surface) {
    ASSERT(surface != NULL);
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
        struct wlr_surface *focused_surface = seat->keyboard_state.focused_surface;
        if (wlr_surface_is_xdg_surface(focused_surface)) {
            struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(focused_surface);
            wlr_xdg_toplevel_set_activated(previous, false);
#ifdef XWAYLAND
        } else if (wlr_surface_is_xwayland_surface(focused_surface)) {
            struct wlr_xwayland_surface *previous = wlr_xwayland_surface_from_wlr_surface(focused_surface);
            wlr_xwayland_surface_activate(previous, false);
#endif
        } else {
            // Not an error as this could be a layer surface
            wlr_log(WLR_DEBUG, "Could not deactivate previous keyboard-focused surface");
        }
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

/// Handle an absolute cursor motion event. This happens when runing under a Wayland
/// window rather than KMS+DRM.
static void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	struct viv_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(server->cursor, event->device, event->x, event->y);
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

/// Begin a cursor interaction with the given view: this stores the view at server root
/// level so that future cursor movements can be applied to it.
void viv_server_begin_interactive(struct viv_view *view, enum viv_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct viv_server *server = view->server;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (viv_view_get_toplevel_surface(view) != focused_surface) {
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
        viv_view_get_geometry(view, &geo_box);
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

/// True if the global meta key from the config is currently held, else false
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

/// Handle cursor button press event
static void server_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct viv_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_event_pointer_button *event = data;
	double sx, sy;
	struct wlr_surface *surface;
	struct viv_view *view = viv_server_view_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_RELEASED) {
        // End any ongoing grab event
		server->cursor_mode = VIV_CURSOR_PASSTHROUGH;
	} else {
        // Always focus the clicked-on window
		viv_view_focus(view, surface);

        // If making a window floating, always bring it to the front
        if (global_meta_held(server)) {
            viv_view_bring_to_front(view);
            if (event->state == WLR_BUTTON_PRESSED) {
                if (event->button == VIV_LEFT_BUTTON) {
                    viv_server_begin_interactive(view, VIV_CURSOR_MOVE, 0);
                } else if (event->button == VIV_RIGHT_BUTTON) {
                    viv_server_begin_interactive(view, VIV_CURSOR_RESIZE, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
                }
            }
        }
	}

    // Only notify the client about the button state if nothing is being dragged
    if (server->cursor_mode == VIV_CURSOR_PASSTHROUGH) {
        wlr_seat_pointer_notify_button(server->seat,
                                       event->time_msec, event->button, event->state);
    }
}

/// Handle cursor axis event, e.g. from scroll wheel
static void server_cursor_axis(struct wl_listener *listener, void *data) {
	struct viv_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);
}

/// Handle cursor frame event. Frame events are sent after normal pointer events to group
/// multiple events together, e.g. to indicate that two axis events happened at the same
/// time.
static void server_cursor_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_server *server =
		wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

/// Respond to a new output becoming available
static void server_new_output(struct wl_listener *listener, void *data) {
	struct viv_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

    wlr_log(WLR_INFO, "New output appeared with name %s, make %s, model %s, serial %s",
            wlr_output->name,
            wlr_output->make,
            wlr_output->model,
            wlr_output->serial);

    // Use the monitor's preferred mode for now
    // TODO: Make this configuarble
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

    viv_output_init(output, server, wlr_output);

    // If there isn't already an active output, we may as well use this one
    if (!server->active_output) {
        server->active_output = output;
    }
}

/// Create a new viv_view to track a new xdg surface
static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct viv_server *server = wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

    // Create a viv_view to track the xdg surface
	struct viv_view *view = calloc(1, sizeof(struct viv_view));
    CHECK_ALLOCATION(view);
    viv_xdg_view_init(view, xdg_surface);
    viv_view_init(view, server);
}

#ifdef XWAYLAND
/// Create a new viv_view to track a new xwayland surface
static void server_new_xwayland_surface(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_xwayland_surface);
    struct wlr_xwayland_surface *xwayland_surface = data;

    // Create a viv_view to track the xdg surface
	struct viv_view *view = calloc(1, sizeof(struct viv_view));
    CHECK_ALLOCATION(view);
    viv_xwayland_view_init(view, xwayland_surface);
    viv_view_init(view, server);
}

static void server_xwayland_ready(struct  wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_server *server = wl_container_of(listener, server, xwayland_ready);
    wlr_xwayland_set_seat(server->xwayland_shell, server->seat);
    wlr_log(WLR_INFO, "XWayland is ready");
}
#endif


/// Create a new viv_layer_view to track a new layer surface
static void server_new_layer_surface(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *layer_surface = data;
    wlr_log(WLR_INFO, "New layer surface with namespace %s", layer_surface->namespace);

    // If the layer surface doesn't specify an output to display on, use the active output
    if (!layer_surface->output) {
        layer_surface->output = server->active_output->wlr_output;
    }

    struct wlr_layer_surface_v1_state *state = &layer_surface->current;

    struct viv_layer_view *layer_view = calloc(1, sizeof(struct viv_layer_view));
    CHECK_ALLOCATION(layer_view);
    viv_layer_view_init(layer_view, server, layer_surface);

    struct viv_output *output = viv_output_of_wlr_output(server, layer_surface->output);
    output->needs_layout = true;

    wlr_log(WLR_INFO, "New layer surface props: layer %d, anchor %d, exclusive %d, margin (%d, %d, %d, %d), desired size (%d, %d), actual size (%d, %d)", state->layer, state->anchor, state->exclusive_zone, state->margin.top, state->margin.right, state->margin.bottom, state->margin.left, state->desired_width, state->desired_height, state->actual_width, state->actual_height);
}

/// Handle a modifier key press event
static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
    // Let the seat know about the key press: it handles all connected keyboards
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->device);

    // Send modifiers to the current client
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->device->keyboard->modifiers);
}

/// Look up a key press in the configured keybindings, and run the bound function if found
static bool handle_keybinding(struct viv_server *server, xkb_keysym_t sym) {
    struct viv_output *output = server->active_output;

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

/// Handle a key press event
static void server_keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	struct viv_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct viv_server *server = keyboard->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = server->seat;

	// Translate libinput keycode -> xkbcommon
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->device->keyboard->xkb_state, keycode, &syms);


    // If the key completes a configured keybinding, run the configured response function
	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	if ((modifiers & server->config->global_meta_key) && event->state == WLR_KEY_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
            wlr_log(WLR_DEBUG, "Key %d pressed", syms[i]);
			handled = handle_keybinding(server, syms[i]);
		}
	}

    // If the key wasn't a shortcut, pass it through to the focused view
	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

/// Handle a new-keyboard event
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

/// Handle a new-pointer event
static void server_new_pointer(struct viv_server *server,
		struct wlr_input_device *device) {
    //  TODO: Maybe set acceleration etc. here (and make that configurable)
	wlr_cursor_attach_input_device(server->cursor, device);
}

/// Handle a new-input event
static void server_new_input(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
        wlr_log(WLR_ERROR, "Received an unrecognised/unhandled new input, type %d", device->type);
		break;
	}

    wlr_log(WLR_INFO, "New input device with name \"%s\"", device->name);

    // Configure the new device, e.g. applying libinput config options
    viv_input_configure(device, server->config->libinput_configs);

    // Let the wlr_seat know what our capabilities are. This information is available to
    // clients. We always support a cursor even if no input device can actually move it.
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

/// Handle a cursor request (i.e. client requesting a specific image)
static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(
			listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
    // Ignore the request if the client isn't currently pointer-focused
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

/// Handle a request to set the selection
static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

/// Handle new xdg toplevel decorations
static void handle_xdg_new_toplevel_decoration(struct wl_listener *listener, void *data) {
    UNUSED(listener);
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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

#ifdef DEBUG
void viv_check_data_consistency(struct viv_server *server) {
    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        DEBUG_ASSERT_EQUAL(output->server, server);
        DEBUG_ASSERT_EQUAL(output->current_workspace->output, output);
    }

    struct viv_workspace *workspace;
    wl_list_for_each(workspace, &server->workspaces, server_link) {
        // A workspace containing views should always have an active view
        if (wl_list_length(&workspace->views) == 0) {
            DEBUG_ASSERT(workspace->active_view == NULL);
            continue;
        }

        // Check that each view in the workspace is back-linked correctly
        struct viv_view *view;
        bool active_view_in_views = false;
        wl_list_for_each(view, &workspace->views, workspace_link) {
            DEBUG_ASSERT(view->workspace == workspace);
            if (view == workspace->active_view) {
                active_view_in_views = true;
            }
        }
        bool active_view_is_null = (workspace->active_view == NULL);
        DEBUG_ASSERT(active_view_in_views || active_view_is_null);

        if (workspace->output != NULL) {
            // Check that output and workspace are linked correctly
            DEBUG_ASSERT(workspace == workspace->output->current_workspace);
        }
    }
}
#endif

/** Initialise the viv_server by setting up all the global state: the wayland display and
    renderer, output layout, event bindings etc.
 */
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
	server->compositor = wlr_compositor_create(server->wl_display, server->renderer);
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
	wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);

#ifdef XWAYLAND
    // Set up the xwayland shell
	server->xwayland_shell = wlr_xwayland_create(server->wl_display, server->compositor, false);
	server->new_xwayland_surface.notify = server_new_xwayland_surface;
	wl_signal_add(&server->xwayland_shell->events.new_surface, &server->new_xwayland_surface);

	server->xwayland_ready.notify = server_xwayland_ready;
	wl_signal_add(&server->xwayland_shell->events.new_surface, &server->xwayland_ready);

    setenv("DISPLAY", server->xwayland_shell->server->display_name, true);
#else
    // Discourage X apps from starting in some other X server
    unsetenv("DISPLAY");
#endif

    server->cursor_mode = VIV_CURSOR_PASSTHROUGH;

    // Set up the layer-shell
    server->layer_shell = wlr_layer_shell_v1_create(server->wl_display);
    server->new_layer_surface.notify = server_new_layer_surface;
    wl_signal_add(&server->layer_shell->events.new_surface, &server->new_layer_surface);

    // Set up the output manager protocol
    server->xdg_output_manager = wlr_xdg_output_manager_v1_create(server->wl_display, server->output_layout);

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

    struct wlr_server_decoration_manager *decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
    server->decoration_manager = decoration_manager;
    wlr_server_decoration_manager_set_default_mode(decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    server->xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server->wl_display);
    server->xdg_decoration_new_toplevel_decoration.notify = handle_xdg_new_toplevel_decoration;
    wl_signal_add(&server->xdg_decoration_manager->events.new_toplevel_decoration, &server->xdg_decoration_new_toplevel_decoration);

    wl_list_init(&server->unmapped_views);

    server->log_state.last_active_output = NULL;
    server->log_state.last_active_workspace = NULL;

    wlr_log(WLR_INFO, "New viv_server initialised");

    viv_parse_and_run_background_config(
        server->config->background.colour,
        server->config->background.image,
        server->config->background.mode
    );

    server->bar_pid = viv_parse_and_run_bar_config(server->config->bar.command, server->config->bar.update_signal_number);

}
