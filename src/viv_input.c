
#include <string.h>
#include <libinput.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>

#include "viv_config_types.h"

static void configure_libinput_device(struct libinput_device *device, struct viv_libinput_config *config) {
    libinput_device_config_scroll_set_method(device, config->scroll_method);
    libinput_device_config_scroll_set_button(device, config->scroll_button);
    libinput_device_config_middle_emulation_set_enabled(device, config->middle_emulation);
    libinput_device_config_left_handed_set(device, config->left_handed);
    libinput_device_config_scroll_set_natural_scroll_enabled(device, config->natural_scroll);
}

static void try_configure_libinput_device(struct wlr_input_device *device, struct viv_libinput_config *libinput_configs) {
    struct libinput_device *li_device = wlr_libinput_get_device_handle(device);

    for (size_t i = 0; i < MAX_NUM_LIBINPUT_CONFIGS; i++) {
        struct viv_libinput_config config = libinput_configs[i];
        if (strlen(config.device_name) == 0) {
            break;
        }

        if (strstr(device->name, config.device_name) != 0) {
            configure_libinput_device(li_device, &config);
        }
    }
}

void viv_input_configure(struct wlr_input_device *device, struct viv_libinput_config *libinput_configs) {
    if (!libinput_configs) {
        // No configs, so do nothing
        return;
    }

    bool is_libinput = wlr_input_device_is_libinput(device);

    if (is_libinput) {
        try_configure_libinput_device(device, libinput_configs);
    } else {
        wlr_log(WLR_DEBUG, "Cannot configure device \"%s\", it isn't a libinput device", device->name);
    }

}
