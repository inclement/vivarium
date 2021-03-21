#ifndef MOCK_LAYOUTS
#define MOCK_LAYOUTS

#include <fff.h>

#include "viv_layout.h"
#include "viv_types.h"

#define MOCK_LAYOUT(LAYOUT_NAME) \
    FAKE_VOID_FUNC(viv_layout_do_ ## LAYOUT_NAME, struct wl_array *, float, uint32_t, uint32_t, uint32_t);

MACRO_FOR_EACH_LAYOUT(MOCK_LAYOUT);


#endif
