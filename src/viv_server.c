#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <libinput.h>
#include <wlr/backend.h>
#ifdef HEADLESS_TEST
#include <wlr/backend/headless.h>
#endif
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
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/util/log.h>
#include <wlr/version.h>
#include <wordexp.h>

#ifdef XWAYLAND
#include <wlr/xwayland.h>
#endif

#include "viv_background.h"
#include "viv_bar.h"
#include "viv_config_types.h"
#include "viv_types.h"
#include "viv_input.h"
#include "viv_server.h"
#include "viv_workspace.h"
#include "viv_layout.h"
#include "viv_output.h"
#include "viv_render.h"
#include "viv_seat.h"
#include "viv_toml_config.h"
#include "viv_layer_view.h"
#include "viv_view.h"
#include "viv_xdg_shell.h"

#ifdef XWAYLAND
#include "viv_xwayland_shell.h"
#endif

#include "viv_debug_support.h"
#include "viv_config.h"
#include "viv_config_support.h"

#define DEFAULT_SEAT_NAME "seat0"

void viv_server_clear_view_from_grab_state(struct viv_server *server, struct viv_view *view) {
    struct viv_seat *seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        if (seat->grab_state.view == view) {
            seat->grab_state.view = NULL;
            seat->cursor_mode = VIV_CURSOR_PASSTHROUGH;
        }
    }
}

bool viv_server_any_seat_grabs(struct viv_server *server, struct viv_view *view) {
    struct viv_seat *seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        if (seat->grab_state.view == view) {
            return true;
        }
    }
    return false;
}

struct viv_workspace *viv_server_retrieve_workspace_by_name(struct viv_server *server, char *name) {
    struct viv_workspace *workspace;
    wl_list_for_each(workspace, &server->workspaces, server_link) {
        if (strcmp(workspace->name, name) == 0) {
            return workspace;
        }
    }
    wlr_log(WLR_ERROR, "Could not find workspace with name \"%s\"", name);
    return NULL;
}

struct viv_layer_view *viv_server_layer_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy,
        uint32_t layers) {

    if (!server->active_output) {
        // No active output => nothing to collide with
        return NULL;
    }

    if (wl_list_length(&server->active_output->layer_views)) {
        struct viv_layer_view *layer_view;
        wl_list_for_each(layer_view, &server->active_output->layer_views, output_link) {

            if (viv_layer_view_layer_in(layer_view, layers) &&
                viv_layer_view_is_at(layer_view, lx, ly, surface, sx, sy)) {
                return layer_view;
            }
        }
    }
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

    if (!active_output) {
        // If there's no active output, there's no collision check todo
        // TODO: Consider querying the output layout for the current output
        return NULL;
    }

    // Fullscreen view should always be blocking all other views
    if (active_output->current_workspace->fullscreen_view) {
      if (!viv_view_is_at(active_output->current_workspace->fullscreen_view, lx, ly, surface, sx, sy)) {
          *surface = NULL;
      }
      return active_output->current_workspace->fullscreen_view;
    }

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
    // First check the active view, as this will be drawn on top in the case of overlap
    struct viv_view *active_view = server->active_output->current_workspace->active_view;
    if (active_view && viv_view_is_at(active_view, lx, ly, surface, sx, sy)) {
        return active_view;
    }
	wl_list_for_each(view, &server->active_output->current_workspace->views, workspace_link) {
		if (viv_view_is_at(view, lx, ly, surface, sx, sy)) {
			return view;
		}
	}
	return NULL;
}

/// Respond to a new output becoming available
static void server_new_output(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_output);
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
        viv_output_make_active(output);
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

    xdg_surface->data = view;
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

    xwayland_surface->data = view;
}

static void server_xwayland_ready(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_server *server = wl_container_of(listener, server, xwayland_ready);
    struct viv_seat *seat = viv_server_get_default_seat(server);
    wlr_xwayland_set_seat(server->xwayland_shell, seat->wlr_seat);
    wlr_log(WLR_INFO, "XWayland is ready");

    // Get the xcb_atom_t for all the X properties we care about, for later lookup
    viv_xwayland_lookup_atoms(server);
}
#endif


/// Create a new viv_layer_view to track a new layer surface
static void server_new_layer_surface(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_layer_surface);
    struct wlr_layer_surface_v1 *layer_surface = data;
    wlr_log(WLR_INFO, "New layer surface with namespace %s", layer_surface->namespace);

    // If the layer surface doesn't specify an output to display on, use the active output
    if (!layer_surface->output) {
        struct viv_output *active_output = server->active_output;
        if (active_output) {
            layer_surface->output = active_output->wlr_output;
        } else {
            wlr_log(WLR_ERROR, "Closing new layer surface as no output available to display it on");
            layer_surface->output = NULL;
            wlr_layer_surface_v1_destroy(layer_surface);
        }
    }

    struct wlr_layer_surface_v1_state *state = &layer_surface->current;

    struct viv_layer_view *layer_view = calloc(1, sizeof(struct viv_layer_view));
    CHECK_ALLOCATION(layer_view);
    viv_layer_view_init(layer_view, server, layer_surface);

    struct viv_output *output = viv_output_of_wlr_output(server, layer_surface->output);
    viv_output_mark_for_relayout(output);

    layer_surface->data = layer_view;

    wlr_log(WLR_INFO, "New layer surface props: layer %d, anchor %d, exclusive %d, margin (%d, %d, %d, %d), desired size (%d, %d), actual size (%d, %d)", state->layer, state->anchor, state->exclusive_zone, state->margin.top, state->margin.right, state->margin.bottom, state->margin.left, state->desired_width, state->desired_height, state->actual_width, state->actual_height);
}

/// Handle builtin keybindings that cannot be overridden
static bool handle_server_keybinding(struct viv_server *server, xkb_keysym_t sym) {

    // Handle TTY switching
    if ((sym >= XKB_KEY_XF86Switch_VT_1) && (sym <= XKB_KEY_XF86Switch_VT_12)) {
        struct wlr_session *session = wlr_backend_get_session(server->backend);
        if (session) {
            unsigned int vt_num = sym - XKB_KEY_XF86Switch_VT_1 + 1;
            wlr_session_change_vt(session, vt_num);
            return true;
        }
    }
    return false;
}

/// Look up a key press in the configured keybindings, and run the bound function if found
bool viv_server_handle_keybinding(struct viv_server *server, uint32_t keycode, xkb_keysym_t sym, uint32_t modifiers) {
    struct viv_output *output = server->active_output;
    if (!output) {
        wlr_log(WLR_ERROR, "Ignoring keybinding as no output active to act on");
    }

    struct viv_workspace *workspace = output->current_workspace;

    if (handle_server_keybinding(server, sym)) {
        return true;
    }

    struct viv_keybind keybind;
    for (uint32_t i = 0; i < MAX_NUM_KEYBINDS; i++) {
        keybind = server->config->keybinds[i];

        if (keybind.key == NULL_KEY) {
            break;
        }

        bool all_modifiers_pressed = false;

        bool correct_key_pressed = false;
        switch (keybind.type) {
        case VIV_KEYBIND_TYPE_KEYCODE:
            // For keycodes, we have to care about all modifiers equally
            all_modifiers_pressed = (keybind.modifiers == modifiers);
            correct_key_pressed = (keybind.keycode == keycode);
            break;
        case VIV_KEYBIND_TYPE_KEYSYM:
            if (keybind.modifiers & WLR_MODIFIER_SHIFT) {
                // If shift is explicitly listed in the keybind, require it to be present
                all_modifiers_pressed = (keybind.modifiers == modifiers);
            } else {
                // If shift wasn't explicitly listed in the keybind, assume it's allowable
                // as for most keys pressing shift has also changed the keysym
                all_modifiers_pressed = ((keybind.modifiers | WLR_MODIFIER_SHIFT) == (modifiers | WLR_MODIFIER_SHIFT));
            }
            correct_key_pressed = (keybind.key == sym);
            break;
        default:
            UNREACHABLE();
        }

        if (all_modifiers_pressed && correct_key_pressed) {
            wlr_log(WLR_INFO, "Found keybind: type %d, keycode %d, sym %d, modifiers %d, pressed modifiers %d, binding %p",
                    keybind.type, keycode, sym, keybind.modifiers, modifiers, keybind.binding);
            keybind.binding(workspace, keybind.payload);
            return true;
        }
    }
    return false;
}


/// Handle a new-pointer event
static void server_new_pointer(struct viv_server *server,
		struct wlr_input_device *device) {
    //  TODO: Maybe set acceleration etc. here (and make that configurable)
    struct viv_seat *seat = viv_server_get_default_seat(server);
	wlr_cursor_attach_input_device(seat->cursor, device);
}

/// Handle a new-input event
static void server_new_input(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
        viv_seat_create_new_keyboard(viv_server_get_default_seat(server), device);
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

    // Let the wlr_seat(s) know what our capabilities are. This information is available to
    // clients. We always support a cursor even if no input device can actually move it.
    struct viv_seat *seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!wl_list_empty(&seat->keyboards)) {
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        }
        wlr_seat_set_capabilities(seat->wlr_seat, caps);
    }
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
                            struct viv_layout *layouts,
                            struct viv_server *server) {
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

        workspace->server = server;

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
        if (output->current_workspace) {
            DEBUG_ASSERT_EQUAL(output->current_workspace->output, output);
        }
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

static char *get_default_config_path(void) {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    char *config_path;
    if (xdg_config_home && *xdg_config_home) {
        config_path = "$XDG_CONFIG_HOME/vivarium/config.toml";
    } else {
        config_path ="$HOME/.config/vivarium/config.toml";
    }

    wordexp_t exp;
    if (wordexp(config_path, &exp, WRDE_UNDEF | WRDE_SHOWERR) == 0) {
        char *path = strdup(exp.we_wordv[0]);
        wordfree(&exp);
        return path;
    }
    return NULL;
}

void load_toml_config(struct viv_config *config, char *user_path) {
    char *config_search_path = get_default_config_path();
    if (user_path) {
        viv_toml_config_load(user_path, config, true);
    } else if (config_search_path) {
        wlr_log(WLR_DEBUG, "Resolved that default config path is \"%s\"", config_search_path);
        viv_toml_config_load(config_search_path, config, false);
    } else {
        EXIT_WITH_MESSAGE("Could not work out config path for some reason - is $HOME not defined?");
    }
}

void viv_server_reload_config(struct viv_server *server) {
    load_toml_config(server->config, server->user_provided_config_filen);

    struct viv_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        viv_output_mark_for_relayout(output);
    }

    // TODO: Reset workspaces and layouts according to new config
}

static void handle_input_inhibit_activate(struct wl_listener *listener, void *data) {
	struct viv_server *server = wl_container_of(listener, server, input_inhibit_activate);

    struct wlr_input_inhibit_manager *inhibit_manager = data;

    wlr_log(WLR_INFO, "Input inhibit activated");

    struct viv_seat* seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        viv_seat_set_exclusive_client(seat, inhibit_manager->active_client);
    }
}

static void handle_input_inhibit_deactivate(struct wl_listener *listener, void *data) {
    UNUSED(data);
	struct viv_server *server = wl_container_of(listener, server, input_inhibit_deactivate);

    wlr_log(WLR_INFO, "Input inhibit deactivated");

    struct viv_seat* seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        viv_seat_set_exclusive_client(seat, NULL);
    }
}

static void handle_output_power_manager_set_mode(struct wl_listener *listener, void *data) {
    UNUSED(listener);

    struct wlr_output_power_v1_set_mode_event *event = data;
    bool enabling = event->mode == ZWLR_OUTPUT_POWER_V1_MODE_ON;

    wlr_log(WLR_INFO, "Setting %s power mode to %d", event->output->name, enabling);

    wlr_output_enable(event->output, enabling);
    if (!wlr_output_commit(event->output)) {
        wlr_log(WLR_ERROR, "Failed to commit %s power mode with value %d", event->output->name, enabling);
    }
}

static void handle_new_idle_inhibitor(struct wl_listener *listener, void *data) {
    struct viv_server *server = wl_container_of(listener, server, new_idle_inhibitor);
    struct wlr_idle_inhibitor_v1 *inhibitor = data;

    wlr_log(WLR_DEBUG, "New idle inhibitor");
    wl_signal_add(&inhibitor->events.destroy, &server->destroy_idle_inhibitor);
    viv_server_update_idle_inhibitor_state(server);
}

static void handle_destroy_idle_inhibitor(struct wl_listener *listener, void *data) {
    struct viv_server *server = wl_container_of(listener, server, destroy_idle_inhibitor);
    UNUSED(data);
    wlr_log(WLR_DEBUG, "Destroying idle inhibitor");
    viv_server_update_idle_inhibitor_state(server);
}

/** Initialise the viv_server by setting up all the global state: the wayland display and
    renderer, output layout, event bindings etc.
 */
void viv_server_init(struct viv_server *server) {
    // Initialise logging, NULL indicates no callback so default logger is used
    wlr_log_init(WLR_DEBUG, NULL);

    // Always start with the config from build-time header
    struct viv_config *config = &the_config;
    // Load other config options from config.toml if possible
    load_toml_config(config, server->user_provided_config_filen);
    // Use the config, in whatever state it's ended up in
    server->config = config;

    // Dynamically create workspaces according to the user configuration
    init_workspaces(&server->workspaces, server->config->workspaces, server->config->layouts, server);

    // Prepare our app's wl_display
	server->wl_display = wl_display_create();

    // Create a wlroots backend. This will automatically handle creating a suitable
    // backend for the environment, e.g. an X11 window if running under X.
#ifdef HEADLESS_TEST
    // Use a headless backend for CI testing without any actual outputs

	server->backend = wlr_headless_backend_create(server->wl_display);
#else
	server->backend = wlr_backend_autocreate(server->wl_display);

#endif

    if (!server->backend) {
        EXIT_WITH_MESSAGE("Failed to create server backend");
    }

    // Init the default wlroots GLES2 renderer
	server->renderer = wlr_backend_get_renderer(server->backend);
	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

    // Create some default wlroots interfaces:
    // Compositor to handle surface allocation
	server->compositor = wlr_compositor_create(server->wl_display, server->renderer);
    // Data device manager to handle the clipboard
	wlr_data_device_manager_create(server->wl_display);

    wlr_screencopy_manager_v1_create(server->wl_display);

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

    // Set up the layer-shell
    server->layer_shell = wlr_layer_shell_v1_create(server->wl_display);
    server->new_layer_surface.notify = server_new_layer_surface;
    wl_signal_add(&server->layer_shell->events.new_surface, &server->new_layer_surface);

    // Set up the output manager protocol
    server->xdg_output_manager = wlr_xdg_output_manager_v1_create(server->wl_display, server->output_layout);

    // Use a wlroots xcursor manager to handle the cursor theme
	server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->cursor_mgr, 1);

    // Set up a wlroots "seat" handling the combination of user input devices
	server->new_input.notify = server_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);

    wl_list_init(&server->seats);
	server->default_seat = viv_seat_create(server, DEFAULT_SEAT_NAME);

    server->idle = wlr_idle_create(server->wl_display);

    // TODO when introducing support for managing multiple displays
    // Take into account that the idle inhibitor manager is bound to one
    server->idle_inhibit_manager = wlr_idle_inhibit_v1_create(server->wl_display);
    wl_signal_add(&server->idle_inhibit_manager->events.new_inhibitor, &server->new_idle_inhibitor);
    server->new_idle_inhibitor.notify = handle_new_idle_inhibitor;
    // handle destroy is added individually to inhibitors
    server->destroy_idle_inhibitor.notify = handle_destroy_idle_inhibitor;

    struct wlr_server_decoration_manager *decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
    server->decoration_manager = decoration_manager;
    wlr_server_decoration_manager_set_default_mode(decoration_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    server->xdg_decoration_manager = wlr_xdg_decoration_manager_v1_create(server->wl_display);
    server->xdg_decoration_new_toplevel_decoration.notify = handle_xdg_new_toplevel_decoration;
    wl_signal_add(&server->xdg_decoration_manager->events.new_toplevel_decoration, &server->xdg_decoration_new_toplevel_decoration);

    server->input_inhibit_manager = wlr_input_inhibit_manager_create(server->wl_display);
    server->input_inhibit_activate.notify = handle_input_inhibit_activate;
    wl_signal_add(&server->input_inhibit_manager->events.activate, &server->input_inhibit_activate);
    server->input_inhibit_deactivate.notify = handle_input_inhibit_deactivate;
    wl_signal_add(&server->input_inhibit_manager->events.deactivate, &server->input_inhibit_deactivate);

    server->output_power_manager = wlr_output_power_manager_v1_create(server->wl_display);
    server->output_power_manager_set_mode.notify = handle_output_power_manager_set_mode;
    wl_signal_add(&server->output_power_manager->events.set_mode, &server->output_power_manager_set_mode);

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

#ifdef XWAYLAND
    struct viv_seat *seat = viv_server_get_default_seat(server);
    wlr_xcursor_manager_set_cursor_image(server->cursor_mgr, "left_ptr", seat->cursor);
    struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(server->cursor_mgr, "left_ptr", 1);
    if (!xcursor) {ASSERT(false);}
    struct wlr_xcursor_image *image = xcursor->images[0];
    wlr_xwayland_set_cursor(server->xwayland_shell,
                            image->buffer, image->width * 4, image->width,
                            image->height, image->hotspot_x,
                            image->hotspot_y);
#endif  // XWAYLAND
}

struct viv_seat *viv_server_get_default_seat(struct viv_server *server) {
    // TODO: Work out how to actually handle transitions between seat events and do
    // something more interesting here when multiple seats are supported
    return server->default_seat;
}

void viv_server_update_idle_inhibitor_state(struct viv_server *server) {
    bool inhibited = false;

    const struct wlr_idle_inhibitor_v1 *inhibitor;
    wl_list_for_each(inhibitor, &server->idle_inhibit_manager->inhibitors, link) {
        if (wlr_surface_is_layer_surface(inhibitor->surface)) {
            struct wlr_layer_surface_v1 *layer_surface = wlr_layer_surface_v1_from_wlr_surface(inhibitor->surface);
            if (layer_surface) {
                struct viv_layer_view *layer_view = layer_surface->data;
                if (layer_view && layer_view->mapped && (layer_view->output == server->active_output)) {
                    inhibited = true;
                    break;
                }
            }
        } else {
            struct viv_view *view = NULL;
            if (wlr_surface_is_xdg_surface(inhibitor->surface)) {
                struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_from_wlr_surface(inhibitor->surface);
                if (xdg_surface) {
                    view = xdg_surface->data;
                }
#ifdef XWAYLAND
            } else if (wlr_surface_is_xwayland_surface(inhibitor->surface)) {
                struct wlr_xwayland_surface *xwayland_surface = wlr_xwayland_surface_from_wlr_surface(inhibitor->surface);
                if (xwayland_surface) {
                    view = xwayland_surface->data;
                }
#endif  // XWAYLAND
            }

            if (view && view->mapped && (view->workspace == server->active_output->current_workspace)) {
                inhibited = true;
                break;
            }
        }
    }

    wlr_idle_set_enabled(server->idle, NULL, !inhibited);
}

void viv_server_deinit(struct viv_server *server) {
    // Clean everything when shutting down
#ifdef XWAYLAND
    wlr_xwayland_destroy(server->xwayland_shell);
#endif

	wl_display_destroy_clients(server->wl_display);
	wl_display_destroy(server->wl_display);
}
