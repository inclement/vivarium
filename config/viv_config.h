#ifndef VIV_CONFIG_H
#define VIV_CONFIG_H

#include <stdlib.h>

#include <wayland-util.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>

#include "viv_types.h"
#include "viv_layout.h"
#include "viv_workspace.h"
#include "viv_view.h"

#include "viv_config_types.h"
#include "viv_config_support.h"

#define META WLR_MODIFIER_ALT  // convenience define as most keybindings use the same meta key

/// Your configuration can include your own functions. This example is bound to a key
/// later in the config.
static void example_user_function(struct viv_workspace *workspace) {
    wlr_log(WLR_INFO, "User-provided function called with workspace at address %p", workspace);
}

/// This example defines a custom tiling layout from scratch: each view takes up a fraction of the
/// remaining horizontal space. This is just an example, not a useful layout.
static void example_user_layout(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height) {
    UNUSED(counter_param);
    uint32_t available_width = width;
    uint32_t x = 0;
    uint32_t y = 0;
    struct viv_view **view_ptr;
    wl_array_for_each(view_ptr, views) {
        struct viv_view *view = *view_ptr;
        uint32_t current_width = available_width * float_param;
        viv_view_set_target_box(view, x, y, current_width, height);
        x += current_width;
        available_width -= current_width;
    }
}


/// Define any number of workspaces via this table. Each definition is a triple of
/// (name string, key to switch to workspace, key to send the active window to workspace).
/// Key names are looked up in xkbcommon-keysyms.h, e.g. `quotedbl` -> `XKB_KEY_quotedbl`.
#define FOR_EACH_WORKSPACE(MACRO)               \
    MACRO("main", 1, exclam)                    \
    MACRO("2", 2, quotedbl)                     \
    MACRO("3", 3, bracketleft)                  \
    MACRO("4", 4, bracketright)                 \
    MACRO("5", 5, percent)                      \
    MACRO("6", 6, caret)                        \
    MACRO("7", 7, ampersand)                    \
    MACRO("8", 8, asterisk)                     \
    MACRO("9", 9, parenleft)

/// Macros used later to set up your workspaces
#define WORKSPACE_NAME(name, _1, _2) \
    name,
#define BIND_SWITCH_TO_WORKSPACE(name, key, _) \
    KEYBIND_MAPPABLE(META, key, switch_to_workspace, .workspace_name = name),
#define BIND_SHIFT_ACTIVE_WINDOW_TO_WORKSPACE(name, _, key) \
    KEYBIND_MAPPABLE(META, key, shift_active_window_to_workspace, .workspace_name = name),


/// Declare any number of keybindings.
/// The list of available commands and arguments can be found in `viv_mappable_functions.h`.
/// Key names are looked up in xkbcommon-keysyms.h, e.g. `E` -> `XKB_KEY_E`.
/// Keybindings are always accessed via the global meta key, see later configuration.
#define CONFIG_TERMINAL "alacritty"
#define CONFIG_SPACER_INCREMENT 0.05
struct viv_keybind the_keybinds[] = {
    // Note the META key is defined above
    // The first argument is a bitmask, so you can require any set of modifiers from wlr_keyboard.h
    KEYBIND_MAPPABLE(META, Q, terminate),
    KEYBIND_MAPPABLE(META, T, do_exec, .executable = CONFIG_TERMINAL),
    KEYBIND_MAPPABLE(META | WLR_MODIFIER_SHIFT, Return, do_exec, .executable = CONFIG_TERMINAL),
    KEYBIND_MAPPABLE(META, l, increment_divide, .increment = CONFIG_SPACER_INCREMENT),
    KEYBIND_MAPPABLE(META, h, increment_divide, .increment = -CONFIG_SPACER_INCREMENT),
    KEYBIND_MAPPABLE(META, comma, increment_counter, .increment = +1),
    KEYBIND_MAPPABLE(META, period, increment_counter, .increment = -1),
    KEYBIND_MAPPABLE(META, j, next_window),
    KEYBIND_MAPPABLE(META, k, prev_window),
    KEYBIND_MAPPABLE(META, J, shift_active_window_down),
    KEYBIND_MAPPABLE(META, K, shift_active_window_up),
    KEYBIND_MAPPABLE(META, t, tile_window),
    KEYBIND_MAPPABLE(META, e, right_output),
    KEYBIND_MAPPABLE(META, w, left_output),
    KEYBIND_MAPPABLE(META, space, next_layout),
    KEYBIND_MAPPABLE(META, E, shift_active_window_to_right_output),
    KEYBIND_MAPPABLE(META, W, shift_active_window_to_left_output),
    KEYBIND_MAPPABLE(META, C, close_window),
    KEYBIND_MAPPABLE(META, Return, make_window_main),
    KEYBIND_MAPPABLE(META, R, reload_config),
    KEYBIND_MAPPABLE(META, o, do_exec, .executable = "bemenu-run"),
    /// How to run any shell command:
    KEYBIND_MAPPABLE(NO_MODIFIERS, XF86AudioMute, do_shell, .command = "pactl set-sink-mute 0 toggle"),
    KEYBIND_MAPPABLE(NO_MODIFIERS, XF86AudioLowerVolume, do_shell, .command = "amixer -q sset Master 3%-"),
    KEYBIND_MAPPABLE(NO_MODIFIERS, XF86AudioRaiseVolume, do_shell, .command = "amixer -q sset Master 3%+"),
    /// How to bind your own function rather than an existing command:
    KEYBIND_USER_FUNCTION(META, F, &example_user_function),
    KEYBIND_MAPPABLE(META, d, debug_damage_all),
    KEYBIND_MAPPABLE(META, p, debug_swap_buffers), /*  */
    /// Autogenerate keybindings to switch to and/or send windows to each workspace:
    FOR_EACH_WORKSPACE(BIND_SWITCH_TO_WORKSPACE)
    FOR_EACH_WORKSPACE(BIND_SHIFT_ACTIVE_WINDOW_TO_WORKSPACE)
    TERMINATE_KEYBINDS_LIST()  // Do not delete!
};


/// Declare any number of libinput device configurations
/// Whenever a new libinput device is detected, every device config whose device_name is a
/// substring of the new device name will be applied to that device.
/// This example config sets every possible option, for reference.
struct viv_libinput_config the_libinput_configs[] = {
    // This example config can be safely deleted.
    {
        // This config will match any libinput device whose name contains the following substring.
        .device_name = "Logitech USB Trackball",
        // scroll_method options can be found in libinput.h
        .scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN,
        // Only used if the scroll method is as above
        .scroll_button = 8,
        // Emulate middle click when left and right buttons are clicked simultaneously
        .middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED,
        // Switch left and right buttons
        .left_handed = 0,
        // Invert scroll direction
        .natural_scroll = 0,
    },
    TERMINATE_LIBINPUT_CONFIG_LIST(),
};


/// Declare the layouts you want to use. All workspaces have the same layouts, initially cloned from
/// this initial config although their parameters may change independently at runtime.
#define CONFIG_LAYOUT_PARAMETER_DEFAULT 0.666
#define CONFIG_LAYOUT_COUNTER_DEFAULT 1
struct viv_layout the_layouts[] = {
    /// Each layout must define the following parameters:
    {
        // The layout name is used only for logging or status bar reporting, choose anything
        .name = "Tall",
        // The `layout_function` takes a list of windows to be tiled, and a bounding rectangle.
        // Provide your own or use any from `viv_layout.h`.
        .layout_function = &viv_layout_do_split,
        // A floating point parameter from 0-1. Can be used in any way by a layout but
        // usually controls the fraction of the screen occupied by the main layout region.
        // Can be adjusted at runtime by appropriate keybindings.
        .parameter = CONFIG_LAYOUT_PARAMETER_DEFAULT,
        // An integer parameter equal to or larger than 0. Can be used in any way by a
        // layout but usually controls the number of windows sharing the space
        // allocated to the main layout region.
        .counter = CONFIG_LAYOUT_COUNTER_DEFAULT,
        // A boolean controlling whether window borders are drawn. If true, borders are
        // not drawn for any window in the layout, nor is space reserved for them.
        .no_borders = false,
        // A boolean controlling whether excluded regions (e.g. taskbars) are considered
        // during layout. If true, windows will be drawn over/under excluded regions
        // (depending on e.g. layer shell ordering)
        .ignore_excluded_regions = false,
    },
    {
        .name = "Fullscreen",
        .layout_function = &viv_layout_do_fullscreen,
        .parameter = CONFIG_LAYOUT_PARAMETER_DEFAULT,
        .counter = CONFIG_LAYOUT_COUNTER_DEFAULT,
        .no_borders = true,
    },
    {
        .name = "Fullscreen No Borders",
        .layout_function = &viv_layout_do_fullscreen,
        .parameter = CONFIG_LAYOUT_PARAMETER_DEFAULT,
        .counter = CONFIG_LAYOUT_COUNTER_DEFAULT,
        .ignore_excluded_regions = true,
        .no_borders = true,
    },
    // Example user-defined layout. You probably want to delete this from the config, but
    // this is how to make your own layout.
    {
        .name = "User defined layout",
        .layout_function = &example_user_layout,
        .parameter = CONFIG_LAYOUT_PARAMETER_DEFAULT,
        .counter = CONFIG_LAYOUT_COUNTER_DEFAULT,
    },
    TERMINATE_LAYOUTS_LIST()  // Do not delete!
};


/// The primary application config.
/// Every possible option is set below. To make your own config you only need to update these defaults.
/// See `viv_types.h` for the raw struct declaration.
static struct viv_config the_config = {

    // Modifier key used for all keybindings.
    // More sophisticated keybindings with multiple or different modifiers are not yet supported.
    // Available modifiers can be found in `wlr_keyboard.h`.
    .global_meta_key = META,

    // Focus follows mouse: many window managers default to false, but true works well for tiling WMs.
    .focus_follows_mouse = true,

    // Buttons to use for interaction with floating windows.
    // Available choices can be found in `viv_types.h`.
    .win_move_cursor_button = VIV_LEFT_BUTTON,
    .win_resize_cursor_button = VIV_RIGHT_BUTTON,

    // Configure window borders.
    .border_width = 2,  // defined in pixels
    .active_border_colour = { 1, 0, 0.7, 1 },  // Active window border colour: RGBA in 0-1 range
    .inactive_border_colour = { 0.6, 0.6, 0.9, 1 },  // Inactive window colour: RGBA in 0-1 range

    // Configure gaps between windows
    .gap_width = 0,  // defined in pixels

    // The overall Vivarium background colour, RGBA in the 0-1 range.
    // This may be overridden by the more sophisticated background settings below.
    .clear_colour = { 0.73, 0.73, 0.73, 1.0 },

    // Background configuration. Applies to all outputs. Requires `swaybg` to be installed.
    .background = {
        .colour = "#bbbbbb",  // note: this is overridden by .image if present
        .image = "/path/to/your/background.png",
        .mode = "fill",  // options from swaybg: stretch, fit, fill, center, tile, solid_color
    },

    // Use the keybinds list configured above.
    .keybinds = the_keybinds,

    // Use the layouts list configured above.
    .layouts = the_layouts,

    // The list of workspace names, displayed in logs and the status bar.
    // It is possible to directly pass a list of strings, but then keybindings must be set up manually.
    .workspaces = { FOR_EACH_WORKSPACE(WORKSPACE_NAME) },

    // Keyboard options.
    // It is possible to set multiple toggling layouts via the normal xkb syntax (comma-separated)
	.xkb_rules = {
        .rules = NULL,  // included for completion
        .model = NULL,  // e.g. "pc104"
        .layout = NULL,  // e.g. "us"
        .variant = NULL,  // e.g. "colemak"
        .options = NULL,  // e.g. "ctrl:nocaps"
    },

    // Use the libinput configs list configured above.
    .libinput_configs = the_libinput_configs,

    // Filename at which to write a workspace status string each time the workspace state changes.
    // This exists for basic inter-process communication e.g. with waybar, see below
    .ipc_workspaces_filename = NULL,

    // Status bar configuration.
    .bar = {
        .command = "waybar",  // will be run when Vivarium starts
        .update_signal_number = 1,  // if non-zero, Vivarium sends (SIGRTMIN + update_signal_number)
                                    // to the bar process each time the workspace config changes,
                                    // can be used by the bar process as an update trigger
    },

    // The damage tracking mode: NONE to fully render every frame, FRAME to render only
    // frames with any damage, FULL to render only damaged regions of damaged frames.
    // Note: the default is currently FRAME because FULL damage tracking may still be buggy
    .damage_tracking_mode = VIV_DAMAGE_TRACKING_FRAME,

    // Debug options, not useful outside development:
    .debug_mark_views_by_shell = false,  // mark xdg windows with a green rect, xwayland by a red rect
    .debug_mark_active_output = false,  // draw a blue rectangle in the top left of the active output
    .debug_mark_undamaged_regions = false,  // draw only damaged regions leaving rest of output red
    .debug_mark_frame_draws = false,  // draw a small square that cycles through red/green/blue on frame draw
};


#endif
