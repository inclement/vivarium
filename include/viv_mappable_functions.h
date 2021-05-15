#ifndef VIV_MAPPABLE_FUNCTIONS_H
#define VIV_MAPPABLE_FUNCTIONS_H

#include "viv_types.h"

// Forward-declare in order to be able to declare the individual union members alongside the
// functions they relate to. This is easier to read than declaring the payload structs separately
// from their functions.
union viv_mappable_payload;

#define GENERATE_PAYLOAD_STRUCT(FUNCTION_NAME, ...)                 \
    struct viv_mappable_payload_ ## FUNCTION_NAME { uint8_t _empty; __VA_ARGS__ };
#define GENERATE_DECLARATION(FUNCTION_NAME, DOC, ...)                    \
    void viv_mappable_ ## FUNCTION_NAME(struct viv_workspace *workspace, union viv_mappable_payload payload); \
    GENERATE_PAYLOAD_STRUCT(FUNCTION_NAME, __VA_ARGS__)

#define GENERATE_UNION_ENTRY(FUNCTION_NAME, DOC, ...) struct viv_mappable_payload_ ## FUNCTION_NAME FUNCTION_NAME ;

// Mappable functions have a specific form to make them easy for a user to bind to keys, but this
// also means we need to generate both a declaration and a union parameter to pass to their generic
// keyword argument mechanism. This macro defines the table of mappable functions.
#define MACRO_FOR_EACH_MAPPABLE(MACRO)                                       \
    MACRO(do_exec, "Run an executable. Args: executable (string)", char executable[100]; char *args[100];) \
    MACRO(do_shell, "Run a shell command. Args: command (string)", char command[100];) \
    MACRO(increment_divide, "Increment the layout float parameter. Args: increment (float)", float increment; ) \
    MACRO(increment_counter, "Increment the layout counter parameter. Args: increment (int)", uint32_t increment; ) \
    MACRO(terminate, "Terminate vivarum")                           \
    MACRO(next_window, "Move focus to next window")                 \
    MACRO(prev_window, "Move focus to previous window")             \
    MACRO(shift_active_window_down, "Swap active window with next in the layout") \
    MACRO(shift_active_window_up, "Swap active window with previous in the layout") \
    MACRO(tile_window, "Make window tiling (does nothing for non-floating windows)") \
    MACRO(next_layout, "Switch workspace to next layout")               \
    MACRO(prev_layout, "Switch workspace to previous layout")           \
    MACRO(right_output, "Switch focus to output to the right of current") \
    MACRO(left_output, "Switch focus to output to the left of current") \
    MACRO(switch_to_workspace, "Switch to workspace with given name. Args: workspace_name (string)", char workspace_name[100];) \
    MACRO(shift_active_window_to_workspace, "Move active window to given workspace. Args: workspace_name (string)", char workspace_name[100];) \
    MACRO(shift_active_window_to_right_output, "Move active window to output to the right of current") \
    MACRO(shift_active_window_to_left_output, "Move active window to output to the left of current") \
    MACRO(close_window, "Close active window")                          \
    MACRO(make_window_main, "Move active window to first position in current layout") \
    MACRO(reload_config, "Reload the config.toml (warning: may have weird results, restart vivarium if possible)") \
    MACRO(user_function, "Run a C function. Args: function (pointer (*function)(struct viv_workspace *workspace))", void (*function)(struct viv_workspace *workspace);) \
    MACRO(debug_damage_all, "Mark all outputs as damaged to force a full redraw") \
    MACRO(debug_swap_buffers, "Swap buffers") \
    MACRO(debug_toggle_show_undamaged_regions, "Debug option to draw undamaged regions as red") \

// Declare each mappable function and generate a payload struct to pass as its argument
MACRO_FOR_EACH_MAPPABLE(GENERATE_DECLARATION)

union viv_mappable_payload {
    // Pack all the possible payload structs into a union type
    MACRO_FOR_EACH_MAPPABLE(GENERATE_UNION_ENTRY)
};

#undef GENERATE_DECLARATION
#undef GENERATE_UNION_ENTRY

# endif
