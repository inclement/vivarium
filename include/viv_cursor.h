#ifndef VIV_CURSOR_H
#define VIV_CURSOR_H

#include "viv_types.h"

void viv_cursor_process_cursor_motion(struct viv_server *server, uint32_t time);

/// Give pointer focus to whatever window is currently beneath the cursor (if any)
void viv_cursor_reset_focus(struct viv_server *server, uint32_t time);

#endif
