#ifndef VIV_TYPES_H
#define VIV_TYPES_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>
#include <xkbcommon/xkbcommon.h>

enum viv_cursor_mode {
	VIV_CURSOR_PASSTHROUGH,
	VIV_CURSOR_MOVE,
	VIV_CURSOR_RESIZE,
};

enum cursor_buttons {
    VIV_LEFT_BUTTON = 272,
    VIV_RIGHT_BUTTON = 273,
    VIV_MIDDLE_BUTTON = 274,
};

struct viv_global_config {
    xkb_keysym_t mod_key;  /// Global modifier key
};

struct viv_server {
    struct viv_config *config;

	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list views;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum viv_cursor_mode cursor_mode;
	struct viv_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

    struct wl_list workspaces;
};

struct viv_keybindings {
    struct wl_list bindings;
};


struct viv_workspace;

struct viv_output {
	struct wl_list link;
	struct viv_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;

    uint32_t needs_layout;
    struct viv_workspace *current_workspace;
};

struct viv_layout {
    char name[100];
    void (*layout_function)(struct wl_list views, struct viv_output);  /// Function that applies the layout
    struct wl_list link;
};

struct viv_workspace {
    char name[100];
    struct wl_list layouts;  /// List of layouts available in this workspace
    struct viv_layout current_layout;

    float divide;

    bool needs_layout;

    struct viv_output *output;

    void (*do_layout)(struct viv_workspace *workspace);

    struct wl_list views;  /// Ordered list of views associated with this workspace

    struct wl_list server_link;
};

enum viv_view_type {
    VIV_VIEW_TYPE_XDG_SHELL,
};

struct viv_view {
    enum viv_view_type type;

	struct wl_list link;
	struct wl_list workspace_link;

	struct viv_server *server;
    struct viv_workspace *workspace;
	struct wlr_xdg_surface *xdg_surface;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	bool mapped;
	int x, y;

    bool is_floating;
};

struct viv_keyboard {
	struct wl_list link;
	struct viv_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};


#endif
