#ifndef MOCK_MAPPABLE_FUNCTIONS
#define MOCK_MAPPABLE_FUNCTIONS

#include <fff.h>

#include "viv_mappable_functions.h"
#include "viv_types.h"

#define MOCK_MAPPABLE_FUNCTION(MAPPABLE_NAME, ...)                      \
    FAKE_VOID_FUNC(viv_mappable_ ## MAPPABLE_NAME, struct viv_worksapce *, union viv_mappable_payload);

MACRO_FOR_EACH_MAPPABLE(MOCK_MAPPABLE_FUNCTION);


#endif
