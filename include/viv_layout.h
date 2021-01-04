#ifndef VIV_LAYOUT_H
#define VIV_LAYOUT_H

#include "viv_types.h"

void viv_layout_do_split(struct viv_workspace *workspace, int32_t width, int32_t height);

void viv_layout_do_fullscreen(struct viv_workspace *workspace, int32_t width, int32_t height);

#endif
