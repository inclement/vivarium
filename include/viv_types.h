#ifndef VIV_TYPES_H
#define VIV_TYPES_H

#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <xkbcommon/xkbcommon.h>

#include "viv_config_support.h"

#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include "viv_xwayland_types.h"
#endif

enum viv_damage_tracking_mode {
    VIV_DAMAGE_TRACKING_NONE,  // every frame is fully re-rendered
    VIV_DAMAGE_TRACKING_FRAME,   // any damage triggers a full frame render
    VIV_DAMAGE_TRACKING_FULL,  // only damaged regions are rendered
    VIV_DAMAGE_TRACKING_MAX,
};

enum viv_cursor_mode {
	VIV_CURSOR_PASSTHROUGH,  /// Pass through cursor data to views
	VIV_CURSOR_MOVE,  /// A view is being moved
	VIV_CURSOR_RESIZE,  /// A view is being resized
};

enum cursor_buttons {
    VIV_LEFT_BUTTON = 272,
    VIV_RIGHT_BUTTON = 273,
    VIV_MIDDLE_BUTTON = 274,
};

struct viv_output;  // Forward declare for use by viv_server
struct viv_view;

struct viv_server {
    char *user_provided_config_filen;
    struct viv_config *config;

	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
    struct wlr_compositor *compositor;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;

#ifdef XWAYLAND
    struct wlr_xwayland *xwayland_shell;
    xcb_atom_t window_type_atoms[WINDOW_TYPE_ATOM_MAX];
    struct wl_listener new_xwayland_surface;
    struct wl_listener xwayland_ready;
#endif

    struct wlr_layer_shell_v1 *layer_shell;
    struct wl_listener new_layer_surface;

	struct wlr_xcursor_manager *cursor_mgr;

	struct viv_seat *default_seat;
    struct wl_list seats;  // server_link

    struct wlr_idle *idle;

    struct wlr_idle_inhibit_manager_v1 *idle_inhibit_manager;
    struct wl_listener new_idle_inhibitor;
    struct wl_listener destroy_idle_inhibitor;

	struct wl_listener new_input;

    struct wlr_xdg_output_manager_v1 *xdg_output_manager;

    struct wlr_server_decoration_manager *decoration_manager;

    struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
    struct wl_listener xdg_decoration_new_toplevel_decoration;

    struct wlr_input_inhibit_manager *input_inhibit_manager;
    struct wl_listener input_inhibit_activate;
    struct wl_listener input_inhibit_deactivate;

	struct wlr_output_layout *output_layout;
    struct viv_output *active_output;
	struct wl_list outputs;
	struct wl_listener new_output;

    struct wlr_output_power_manager_v1 *output_power_manager;
    struct wl_listener output_power_manager_set_mode;

    struct wl_list workspaces;

    pid_t bar_pid;

    /// State relating to changes that should be logged
    struct {
        struct viv_output *last_active_output;
        struct viv_workspace *last_active_workspace;
    } log_state;

    /// Unmapped views are not kept within the workspace view lists,
    /// in order to keep things simple when iterating through them
    struct wl_list unmapped_views;
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
	struct wl_listener damage_event;
	struct wl_listener present;
	struct wl_listener enable;
	struct wl_listener mode;
	struct wl_listener destroy;

    struct wlr_output_damage *damage;

    bool needs_layout;
    struct viv_workspace *current_workspace;

    uint32_t frame_draw_count;  // only used by debug options

    struct wl_list layer_views;
    struct {
        uint32_t left;
        uint32_t right;
        uint32_t top;
        uint32_t bottom;
    } excluded_margin;
};

struct viv_layout {
    char name[100];
    void (*layout_function)(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height);  /// Function that applies the layout

    float parameter;  /// A float between 0-1 which the user may configure at runtime
    uint32_t counter;  /// User-configurable uint which the user may configure at runtime, effectively unbounded

    bool no_borders;  /// If true, don't draw borders around windows (including the active window)
    bool ignore_excluded_regions;  /// If true, ignore regions marked excluded by other apps such as taskbars

    struct wl_list workspace_link;
};

struct viv_layer_view {
    struct wlr_layer_surface_v1 *layer_surface;
    struct viv_server *server;
    struct viv_output *output;

    struct viv_surface_tree_node *surface_tree;

    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener new_popup;
    struct wl_listener surface_commit;
    bool mapped;

    struct wl_list output_link;

    int x, y;
};

enum viv_view_type {
    VIV_VIEW_TYPE_UNKNOWN,
    VIV_VIEW_TYPE_XDG_SHELL,
#ifdef XWAYLAND
    VIV_VIEW_TYPE_XWAYLAND,
#endif
};

struct viv_view_implementation {
    void (*set_size)(struct viv_view *view, uint32_t width, uint32_t height);
    void (*set_pos)(struct viv_view *view, uint32_t x, uint32_t y);
    void (*get_geometry)(struct viv_view *view, struct wlr_box *geo_box);
    void (*set_tiled)(struct viv_view *view, uint32_t edges);
    void (*get_string_identifier)(struct viv_view *view, char *output, size_t max_len);
    void (*set_activated)(struct viv_view *view, bool activated);
    struct wlr_surface *(*get_toplevel_surface)(struct viv_view *view);
    void (*close)(struct viv_view *view);
    bool (*is_at)(struct viv_view *view, double lx, double ly, struct wlr_surface **surface, double *sx, double *sy);
    bool (*oversized)(struct viv_view *view);
    void (*inform_unrequested_fullscreen_change)(struct viv_view *view);
    void (*grow_and_center_fullscreen)(struct viv_view *view);
};

struct viv_xdg_popup {
    struct wlr_xdg_popup *wlr_popup;
    struct viv_xdg_popup *parent_popup;
    struct viv_server *server;

    struct viv_surface_tree_node *surface_tree;

    int *lx;  // pointer to x of parent view/layer-view in layout coords
    int *ly;  // pointer to y of parent view/layer-view in layout coords

    struct wl_listener surface_commit;
    struct wl_listener surface_map;
    struct wl_listener surface_unmap;
    struct wl_listener destroy;
    struct wl_listener new_popup;
};

struct viv_view {
    enum viv_view_type type;

    struct viv_view_implementation *implementation;

	struct wl_list workspace_link;

	struct viv_server *server;
    struct viv_workspace *workspace;

    struct viv_surface_tree_node *surface_tree;

    union {
        struct wlr_xdg_surface *xdg_surface;
        struct wlr_xwayland_surface *xwayland_surface;
    };

    // XDG view bindings
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_minimize;
	struct wl_listener set_title;
	struct wl_listener request_fullscreen;
	bool mapped;
	int x, y;

    // Surface bindings
    struct wl_listener surface_commit;
    struct wl_listener new_xdg_popup;

    // Target positions where the layout is trying to place the view. These boxes describe
    // the geometry that the application should output, they do not include any space for
    // borders or gaps. That padding should be accounted for by whoever sets the target
    // box.
    struct wlr_box target_box;
    struct wlr_box target_box_before_fullscreen;

    bool is_floating;
    float floating_width, floating_height;  /// width and height to be used if the view becomes floating

    bool is_static;  /// true for e.g. X11 right click menus, signals that no borders should be drawn
                     /// and resizing/moving is not allowed
};

struct viv_workspace {
    char name[100];
    struct wl_list layouts;  /// List of layouts available in this workspace
    struct viv_layout *active_layout;

    bool needs_layout;  // true if the layout function needs applying, e.g. in response to a new view
    bool was_laid_out;  // true if the workspace was laid out at the end of the last frame, else false

    struct viv_server *server;
    struct viv_output *output;

    struct wl_list views;  /// Ordered list of views associated with this workspace
    struct viv_view *active_view;  /// The view that currently has focus within the workspace
    struct viv_view *fullscreen_view;  /// The view currently in the fullscreen state, i.e. drawn
                                       /// on top of everything else regardless of the current layout

    struct wl_list server_link;
};

struct viv_keyboard {
	struct wl_list link;
    struct viv_seat *seat;
	struct wlr_input_device *device;

    struct wl_listener destroy;
	struct wl_listener modifiers;
	struct wl_listener key;
};

struct viv_config {
    enum wlr_keyboard_modifier global_meta_key;

    bool focus_follows_mouse;
    enum cursor_buttons win_move_cursor_button;
    enum cursor_buttons win_resize_cursor_button;

    int border_width;
    float active_border_colour[4];
    float inactive_border_colour[4];
    float clear_colour[4];

    int gap_width;

    bool allow_fullscreen;

    char workspaces[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LENGTH];

    struct {
        char *colour;
        char *image;
        char *mode;
    } background;

    char *ipc_workspaces_filename;

    struct {
        char *command;
        uint32_t update_signal_number;
    } bar;

    struct {
        char *rules;
        char *model;
        char *layout;
        char *variant;
        char *options;
    } xkb_rules;

    struct viv_keybind *keybinds;

    struct viv_layout *layouts;

    struct viv_libinput_config *libinput_configs;

    enum viv_damage_tracking_mode damage_tracking_mode;

    bool debug_mark_views_by_shell;
    bool debug_mark_active_output;
    bool debug_mark_frame_draws;
    bool debug_mark_undamaged_regions;
};

struct viv_seat {
    struct viv_server *server;
    struct wlr_seat *wlr_seat;

	enum viv_cursor_mode cursor_mode;

	struct wl_list keyboards;

    /// State relating to any currently-grabbed view
    struct {
        struct viv_view *view;  /// Currently-grabbed view
        double x, y;
        struct wlr_box geobox;
        uint32_t resize_edges;  /// union of ::wlr_edges along which the view is being resized
    } grab_state;

    /// Client that has exclusive focus due to input_inhibit protocol - note this client
    /// is specifically assumed to come from that protocol and is also used to trigger
    /// other protocol behaviours such as ignoring hotkeys
    struct wl_client *exclusive_client;

	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	struct wlr_cursor *cursor;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

    struct wl_listener request_start_drag;
    struct wl_listener start_drag;
    struct wl_listener drag_destroy;

    struct wl_list server_link;
};


#endif
