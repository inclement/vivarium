#ifndef VIV_INPUT_H
#define VIV_INPUT_H

#include <wlr/types/wlr_input_device.h>

#include "viv_config_types.h"

/// Configure the input according to the config instructions (if any)
void viv_input_configure(struct wlr_input_device *device, struct viv_libinput_config *libinput_configs);



#endif
