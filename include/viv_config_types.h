#ifndef VIV_CONFIG_TYPES_H
#define VIV_CONFIG_TYPES_H

#include <xkbcommon/xkbcommon.h>

#include "viv_types.h"
#include "viv_mappable_functions.h"

struct viv_keybind {
    xkb_keysym_t key;
    void (*binding)(struct viv_workspace *workspace, union viv_mappable_payload payload);
    union viv_mappable_payload payload;
};

#endif
