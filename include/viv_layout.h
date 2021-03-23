#ifndef VIV_LAYOUT_H
#define VIV_LAYOUT_H

#include <wayland-util.h>

#include "viv_types.h"


#define MACRO_FOR_EACH_LAYOUT(MACRO)                   \
    MACRO(split, "Main panel on the left, other windows stacked on the right", \
          "          |--------------|---------|\n"                                \
          "          |              |    2    |\n"                                \
          "          |              |---------|\n"                                \
          "          |              |    3    |\n"                                \
          "          |     MAIN     |---------|\n"                                \
          "          |              |    4    |\n"                                \
          "          |              |---------|\n"                                \
          "          |              |    5    |\n"                                \
          "          |--------------|---------|"                                  \
        );                                                              \
    MACRO(fullscreen, "All windows fullscreen, only active window visible", \
          "         |-------------------------|\n"                        \
          "         |                         |\n"                        \
          "         |                         |\n"                        \
          "         |                         |\n"                        \
          "         |          MAIN           |\n"                        \
          "         |                         |\n"                        \
          "         |                         |\n"                        \
          "         |                         |\n"                        \
          "         |-------------------------|\n"                        \
        );                                                              \
    MACRO(fibonacci_spiral, "Each window takes up a certain fraction of the remaining space",\
          "         |---------------|---------|\n"                                \
          "         |               |         |\n"                                \
          "         |               |    2    |\n"                                \
          "         |               |         |\n"                                \
          "         |      MAIN     |----|----|\n"                                \
          "         |               |    | 4  |\n"                                \
          "         |               | 3  |----|\n"                                \
          "         |               |    | 5  |\n"                                \
          "         |---------------|----|----|"                                  \
        );                                                              \
    MACRO(central_column, "Main panel in the centre, other windows stacked on both sides", \
          "         |-------|---------|-------|\n"                        \
          "         |       |         |       |\n"                        \
          "         |   2   |         |   4   |\n"                        \
          "         |       |         |       |\n"                        \
          "         |-------|  MAIN   |-------|\n"                        \
          "         |       |         |       |\n"                        \
          "         |   3   |         |   5   |\n"                        \
          "         |       |         |       |\n"                        \
          "         |-------|---------|-------|\n"                        \
        );                                                              \
    MACRO(columns, "Windows placed in columns, all with equal width", \
          "         |----|----|----|----|-----|\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |MAIN| 2  | 3  | 4  | 5   |\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |    |    |    |    |     |\n"                        \
          "         |----|----|----|----|-----|\n"                        \
        );

#define GENERATE_DECLARATION(LAYOUT_NAME, _1, _2) void viv_layout_do_ ## LAYOUT_NAME(struct wl_array *views, float float_param, uint32_t counter_param, uint32_t width, uint32_t height);

MACRO_FOR_EACH_LAYOUT(GENERATE_DECLARATION);

#undef GENERATE_DECLARATION

void viv_layout_apply(struct viv_workspace *workspace, uint32_t width, uint32_t height);

#endif
