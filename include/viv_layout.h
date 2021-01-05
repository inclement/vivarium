#ifndef VIV_LAYOUT_H
#define VIV_LAYOUT_H

#include "viv_types.h"

void viv_layout_do_split(struct viv_workspace *workspace, uint32_t width, uint32_t height);

void viv_layout_do_fullscreen(struct viv_workspace *workspace, uint32_t width, uint32_t height);

void viv_layout_do_fibonacci_spiral(struct viv_workspace *workspace, uint32_t width, uint32_t height);

#endif
