#ifndef VIV_MAPPABLE_FUNCTIONS_H
#define VIV_MAPPABLE_FUNCTIONS_H

#include "viv_types.h"

// Forward-declare in order to be able to declare the individual union members alongside the
// functions they relate to. This is easier to read than declaring the payload structs separately
// from their functions.
union viv_mappable_payload;

#define GENERATE_PAYLOAD_STRUCT(FUNCTION_NAME, ARGS...) \
    struct viv_mappable_payload_ ## FUNCTION_NAME { ARGS };
#define GENERATE_DECLARATION(FUNCTION_NAME, ARGS...)                    \
    void viv_mappable_ ## FUNCTION_NAME(struct viv_workspace *workspace, union viv_mappable_payload payload); \
    GENERATE_PAYLOAD_STRUCT(FUNCTION_NAME, ARGS)

#define GENERATE_UNION_ENTRY(FUNCTION_NAME) struct viv_mappable_payload_ ## FUNCTION_NAME FUNCTION_NAME


GENERATE_DECLARATION(do_exec, char executable[100]; char *args[100];)
GENERATE_DECLARATION(increment_divide, float increment; )
GENERATE_DECLARATION(terminate)
GENERATE_DECLARATION(swap_out)
GENERATE_DECLARATION(next_window)
GENERATE_DECLARATION(prev_window)
GENERATE_DECLARATION(shift_active_window_down)
GENERATE_DECLARATION(shift_active_window_up)
GENERATE_DECLARATION(tile_window)

union viv_mappable_payload {
    GENERATE_UNION_ENTRY(do_exec);
    GENERATE_UNION_ENTRY(increment_divide);
    GENERATE_UNION_ENTRY(terminate);
    GENERATE_UNION_ENTRY(swap_out);
    GENERATE_UNION_ENTRY(next_window);
    GENERATE_UNION_ENTRY(prev_window);
    GENERATE_UNION_ENTRY(tile_window);
    GENERATE_UNION_ENTRY(shift_active_window_down);
    GENERATE_UNION_ENTRY(shift_active_window_up);
};

# endif
