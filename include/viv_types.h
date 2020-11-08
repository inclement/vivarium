#ifndef VIV_TYPES_H
#define VIV_TYPES_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_box.h>

enum viv_cursor_mode {
	VIV_CURSOR_PASSTHROUGH,
	VIV_CURSOR_MOVE,
	VIV_CURSOR_RESIZE,
};

struct viv_server {
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
};

struct viv_output {
	struct wl_list link;
	struct viv_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
};

struct viv_view {
	struct wl_list link;
	struct viv_server *server;
	struct wlr_xdg_surface *xdg_surface;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	bool mapped;
	int x, y;
};

struct viv_keyboard {
	struct wl_list link;
	struct viv_server *server;
	struct wlr_input_device *device;

	struct wl_listener modifiers;
	struct wl_listener key;
};

#endif
