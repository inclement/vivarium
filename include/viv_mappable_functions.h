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


GENERATE_DECLARATION(do_exec, char executable[100]; char *args[100];)

GENERATE_DECLARATION(increment_divide, float increment; )

GENERATE_DECLARATION(terminate)

GENERATE_DECLARATION(swap_out)

GENERATE_DECLARATION(next_window)

GENERATE_DECLARATION(prev_window)

union viv_mappable_payload {
    struct viv_mappable_payload_do_exec do_exec;
    struct viv_mappable_payload_increment_divide increment_divide;
    struct viv_mappable_payload_terminate terminate;
    struct viv_mappable_payload_swap_out swap_out;
    struct viv_mappable_payload_next_window next_window;
    struct viv_mappable_payload_prev_window prev_window;
};

# endif
