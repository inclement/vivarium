#include <wayland-util.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>

#include "viv_cursor.h"
#include "viv_config_support.h"
#include "viv_seat.h"
#include "viv_server.h"
#include "viv_types.h"
#include "viv_view.h"

/// True if the global meta key from the config is currently held, else false
static bool global_meta_held(struct viv_seat *seat) {
    struct viv_keyboard *keyboard;
    wl_list_for_each(keyboard, &seat->keyboards, link) {
        uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
        if (modifiers & seat->server->config->global_meta_key) {
            return true;
        }
    }
    return false;
}

static struct xkb_keymap *load_keymap(struct xkb_context *context, struct xkb_rule_names *rules) {
	struct xkb_keymap *keymap = xkb_map_new_from_names(context, rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    const char *model_str = rules->model ? rules->model : "";
    const char *layout_str = rules->layout ? rules->layout : "";
    const char *variant_str = rules->variant ? rules->variant : "";
    const char *options_str = rules->options ? rules->options : "";
    if (!keymap) {
        wlr_log(WLR_ERROR,
                "Could not load keymap with model \"%s\", layout \"%s\", variant \"%s\", options \"%s\" - "
                "using default keymap instead (probably qwerty)",
                model_str, layout_str, variant_str, options_str);
        struct xkb_rule_names default_rules = { 0 };
        context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        keymap = xkb_map_new_from_names(context, &default_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    } else {
        wlr_log(WLR_INFO, "Successfully loaded keymap with model \"%s\", layout \"%s\", variant \"%s\", options \"%s\"",
                model_str, layout_str, variant_str, options_str);
    }

    return keymap;
}

/// Handle a modifier key press event
static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    // Let the seat know about the key press: it handles all connected keyboards
	wlr_seat_set_keyboard(keyboard->seat->wlr_seat, keyboard->device);

    // Send modifiers to the current client
	wlr_seat_keyboard_notify_modifiers(keyboard->seat->wlr_seat,
		&keyboard->device->keyboard->modifiers);
}

/// Handle a key press event
static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	struct viv_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct viv_server *server = keyboard->seat->server;
	struct wlr_event_keyboard_key *event = data;
	struct wlr_seat *seat = keyboard->seat->wlr_seat;

	// Translate libinput keycode -> xkbcommon
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->device->keyboard->xkb_state, keycode, &syms);

    // If the key completes a configured keybinding, run the configured response function
	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->device->keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
            wlr_log(WLR_DEBUG, "Keysym %d pressed, keycode %d", syms[i], keycode);
			handled = viv_server_handle_keybinding(server, keycode, syms[i], modifiers);
		}
	}

    // If the key wasn't a shortcut, pass it through to the focused view
	if (!handled) {
		wlr_seat_set_keyboard(seat, keyboard->device);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->link);
}


void viv_seat_create_new_keyboard(struct viv_seat *seat, struct wlr_input_device *device) {
	struct viv_keyboard *keyboard = calloc(1, sizeof(struct viv_keyboard));
    CHECK_ALLOCATION(keyboard);

	keyboard->seat = seat;
	keyboard->device = device;

    struct viv_server *server = seat->server;
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names rules = {
        .rules = server->config->xkb_rules.rules,
        .model = server->config->xkb_rules.model,
        .layout = server->config->xkb_rules.layout,
        .variant = server->config->xkb_rules.variant,
        .options = server->config->xkb_rules.options,
    };
    struct xkb_keymap *keymap = load_keymap(context, &rules);

    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

    // Handle events from the wlr_keyboard
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&device->keyboard->events.key, &keyboard->key);
	wlr_seat_set_keyboard(seat->wlr_seat, device);

    // Handle the destruction of the input device
    keyboard->destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&seat->keyboards, &keyboard->link);
}

void viv_seat_focus_surface(struct viv_seat *seat, struct wlr_surface *surface) {
    ASSERT(surface != NULL);
    struct wlr_seat *wlr_seat = seat->wlr_seat;
	struct wlr_surface *prev_surface = wlr_seat->keyboard_state.focused_surface;
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
        struct wlr_surface *focused_surface = wlr_seat->keyboard_state.focused_surface;
        if (wlr_surface_is_xdg_surface(focused_surface)) {
            struct wlr_xdg_surface *previous = wlr_xdg_surface_from_wlr_surface(focused_surface);
            wlr_xdg_toplevel_set_activated(previous, false);
#ifdef XWAYLAND
        } else if (wlr_surface_is_xwayland_surface(focused_surface)) {
            // TODO: Deactivating the previous xwayland surface appears wrong - it makes
            // chromium popups disappear. Maybe we have some other logic issue.
            /* struct wlr_xwayland_surface *previous = wlr_xwayland_surface_from_wlr_surface(focused_surface); */
            /* wlr_xwayland_surface_activate(previous, false); */
#endif
        } else {
            // Not an error as this could be a layer surface
            wlr_log(WLR_DEBUG, "Could not deactivate previous keyboard-focused surface");
        }
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(wlr_seat);

	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	wlr_seat_keyboard_notify_enter(wlr_seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

/// Begin a cursor interaction with the given view: this stores the view at server root
/// level so that future cursor movements can be applied to it.
void viv_seat_begin_interactive(struct viv_seat *seat, struct viv_view *view, enum viv_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct viv_server *server = view->server;
	struct wlr_surface *focused_surface = seat->wlr_seat->pointer_state.focused_surface;
	if (viv_view_get_toplevel_surface(view) != focused_surface) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grab_state.view = view;
	seat->cursor_mode = mode;

    // Any view can be interacted with, but this automatically pulls it out of tiling
    viv_view_ensure_floating(view);

	if (mode == VIV_CURSOR_MOVE) {
		server->grab_state.x = seat->cursor->x - view->x;
		server->grab_state.y = seat->cursor->y - view->y;
	} else {
        struct wlr_box geo_box;
        viv_view_get_geometry(view, &geo_box);
        double border_x = (geo_box.x) + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
        double border_y = (geo_box.y) + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
        server->grab_state.x = seat->cursor->x - border_x;
        server->grab_state.y = seat->cursor->y - border_y;

        server->grab_state.geobox = geo_box;

        server->grab_state.resize_edges = edges;
	}
}

/// Handle a cursor request (i.e. client requesting a specific image)
static void seat_request_cursor(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = seat->wlr_seat->pointer_state.focused_client;

    // Ignore the request if the client isn't currently pointer-focused
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(seat->cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

/// Handle a request to set the selection
static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
}

/// Handle a cursor motion event
static void seat_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
    struct viv_seat *seat = wl_container_of(listener, seat, cursor_motion);
	struct viv_server *server = seat->server;
	struct wlr_event_pointer_motion *event = data;

    // Pass the movement along (i.e. allow the cursor to actually move)
	wlr_cursor_move(seat->cursor, event->device,
			event->delta_x, event->delta_y);

    // Do our own processing of the motion if necessary
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

/// Handle an absolute cursor motion event. This happens when runing under a Wayland
/// window rather than KMS+DRM.
static void seat_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, cursor_motion_absolute);
	struct viv_server *server = seat->server;
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(seat->cursor, event->device, event->x, event->y);
	viv_cursor_process_cursor_motion(server, event->time_msec);
}

/// Handle cursor button press event
static void seat_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
    struct viv_seat *seat = wl_container_of(listener, seat, cursor_button);
	struct viv_server *server = seat->server;
	struct wlr_event_pointer_button *event = data;
	double sx, sy;
	struct wlr_surface *surface;
	struct viv_view *view = viv_server_view_at(server, seat->cursor->x, seat->cursor->y, &surface, &sx, &sy);

	if (event->state == WLR_BUTTON_RELEASED || !view) {
        // End any ongoing grab event
		seat->cursor_mode = VIV_CURSOR_PASSTHROUGH;
	} else {
        // Always focus the clicked-on window
        struct viv_view *active_view = NULL;
        if (server->active_output) {
            active_view = server->active_output->current_workspace->active_view;
        }

        if (view != active_view) {
            viv_view_focus(view, surface);
        }

        // If making a window floating, always bring it to the front
        if (global_meta_held(seat)) {
            viv_view_bring_to_front(view);
            if (event->state == WLR_BUTTON_PRESSED) {
                if (event->button == VIV_LEFT_BUTTON) {
                    viv_seat_begin_interactive(seat, view, VIV_CURSOR_MOVE, 0);
                } else if (event->button == VIV_RIGHT_BUTTON) {
                    viv_seat_begin_interactive(seat, view, VIV_CURSOR_RESIZE, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
                }
            }
        }
	}

    // Only notify the client about the button state if nothing is being dragged
    if (seat->cursor_mode == VIV_CURSOR_PASSTHROUGH) {
        wlr_seat_pointer_notify_button(seat->wlr_seat, event->time_msec, event->button, event->state);
    }
}

/// Handle cursor axis event, e.g. from scroll wheel
static void seat_cursor_axis(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_event_pointer_axis *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat->wlr_seat,
                                 event->time_msec, event->orientation, event->delta,
                                 event->delta_discrete, event->source);
}

/// Handle cursor frame event. Frame events are sent after normal pointer events to group
/// multiple events together, e.g. to indicate that two axis events happened at the same
/// time.
static void seat_cursor_frame(struct wl_listener *listener, void *data) {
    UNUSED(data);
    struct viv_seat *seat = wl_container_of(listener, seat, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat->wlr_seat);
}

struct viv_seat *viv_seat_create(struct viv_server *server, char *name) {
    struct wl_display *wl_display = server->wl_display;

    struct viv_seat *seat = calloc(1, sizeof(struct viv_seat));
    CHECK_ALLOCATION(seat);

    seat->server = server;

    struct wlr_seat *wlr_seat = wlr_seat_create(wl_display, name);
    CHECK_ALLOCATION(wlr_seat);
    seat->wlr_seat = wlr_seat;

    wl_list_init(&seat->server_link);
    wl_list_insert(server->seats.prev, &seat->server_link);

    wl_list_init(&seat->keyboards);

    seat->cursor_mode = VIV_CURSOR_PASSTHROUGH;

	seat->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&wlr_seat->events.request_set_cursor, &seat->request_cursor);
	seat->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&wlr_seat->events.request_set_selection, &seat->request_set_selection);

    // Create a wlroots cursor for handling the cursor image shown on the screen for this seat
	seat->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

    // Set up events to be able to move our cursor in response to inputs
	seat->cursor_motion.notify = seat_cursor_motion;
	wl_signal_add(&seat->cursor->events.motion, &seat->cursor_motion);
	seat->cursor_motion_absolute.notify = seat_cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute,
			&seat->cursor_motion_absolute);
	seat->cursor_button.notify = seat_cursor_button;
	wl_signal_add(&seat->cursor->events.button, &seat->cursor_button);
	seat->cursor_axis.notify = seat_cursor_axis;
	wl_signal_add(&seat->cursor->events.axis, &seat->cursor_axis);
	seat->cursor_frame.notify = seat_cursor_frame;
	wl_signal_add(&seat->cursor->events.frame, &seat->cursor_frame);

    return seat;
}
