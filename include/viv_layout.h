#ifndef VIV_LAYOUT_H
#define VIV_LAYOUT_H

#include <wayland-util.h>

#include "viv_types.h"


#define MACRO_FOR_EACH_LAYOUT(MACRO)                   \
    MACRO(split);                                      \
    MACRO(columns);                                    \
    MACRO(fullscreen);                                 \
    MACRO(fibonacci_spiral);                           \
    MACRO(central_column);                             \

#define GENERATE_DECLARATION(LAYOUT_NAME) void viv_layout_do_ ## LAYOUT_NAME(struct viv_workspace *workspace, struct wl_array *views, uint32_t width, uint32_t height);

MACRO_FOR_EACH_LAYOUT(GENERATE_DECLARATION);

#undef GENERATE_DECLARATION

void viv_layout_apply(struct viv_workspace *workspace, uint32_t width, uint32_t height);

#endif
