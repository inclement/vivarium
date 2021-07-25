#ifndef VIV_SERVER_H
#define VIV_SERVER_H

#include "viv_types.h"

/// Fully initialise the server, including loading config, setting up all Vivarium state
/// (workspaces, layouts, keybinds...), and initialising wayland/wlroots state ready to
/// run.
void viv_server_init(struct viv_server *server);

/// Deinitialise server-held state where necessary for a clean exit, including XWayland
/// shell if necessary and wayland display
void viv_server_deinit(struct viv_server *server);

struct viv_view *viv_server_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy);

struct viv_layer_view *viv_server_layer_view_at(
		struct viv_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy,
        uint32_t layers);


struct viv_workspace *viv_server_retrieve_workspace_by_name(struct viv_server *server, char *name);

/// Check through all data held by the server to check for consistency, e.g. all
/// view->workspace is equal to the workspace of their workspace_link. This is intended
/// for debugging only, failures just result in logged errors.
void viv_check_data_consistency(struct viv_server *server);

/// Reload the TOML config file
void viv_server_reload_config(struct viv_server *server);

/// Clear the server grab state, i.e. set cursor mode to passthrough and invalidate the
/// grabbed view pointer
void viv_server_clear_grab_state(struct viv_server *server);

/// Look up a key press in the configured keybindings, and run the bound function if found
bool viv_server_handle_keybinding(struct viv_server *server, uint32_t keycode, xkb_keysym_t sym, uint32_t modifiers);

/// Get the default seat against which all inputs are registered by default.
struct viv_seat *viv_server_get_default_seat(struct viv_server *server);

/// Clear the given view from being grabbed by any seat
void viv_server_clear_view_from_grab_state(struct viv_server *server, struct viv_view *view);

/// True if the given view is currently grabbed by any seat, else false
bool viv_server_any_seat_grabs(struct viv_server *server, struct viv_view *view);

void viv_server_update_idle_inhibitor_state(struct viv_server *server);

void viv_server_update_output_manager_config(struct viv_server *server);
#endif
