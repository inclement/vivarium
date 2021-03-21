#ifndef MOCK_MAPPABLE_FUNCTIONS
#define MOCK_MAPPABLE_FUNCTIONS

#include <fff.h>

#include "viv_mappable_functions.h"
#include "viv_types.h"

#define MOCK_MAPPABLE_FUNCTION(MAPPABLE_NAME, ...)                      \
    FAKE_VOID_FUNC(viv_mappable_ ## MAPPABLE_NAME, struct viv_workspace *, union viv_mappable_payload);

#define RESET_MAPPABLE_FUNCTION_MOCK(LAYOUT_NAME, ...)  \
    RESET_FAKE(viv_mappable_ ## LAYOUT_NAME);

MACRO_FOR_EACH_MAPPABLE(MOCK_MAPPABLE_FUNCTION);

#define RESET_MAPPABLE_FUNCTION_FAKES() MACRO_FOR_EACH_MAPPABLE(RESET_MAPPABLE_FUNCTION_MOCK)

#endif
