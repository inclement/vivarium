#ifndef MOCK_LAYOUTS
#define MOCK_LAYOUTS

#include <fff.h>

#include "viv_layout.h"
#include "viv_types.h"

#define MOCK_LAYOUT(LAYOUT_NAME, _1, _2)                                   \
    FAKE_VOID_FUNC(viv_layout_do_ ## LAYOUT_NAME, struct wl_array *, float, uint32_t, uint32_t, uint32_t);

#define RESET_LAYOUT_MOCK(LAYOUT_NAME, _1, _2)     \
    RESET_FAKE(viv_layout_do_ ## LAYOUT_NAME);

MACRO_FOR_EACH_LAYOUT(MOCK_LAYOUT);

#define RESET_LAYOUT_FAKES() MACRO_FOR_EACH_LAYOUT(RESET_LAYOUT_MOCK)

#endif
