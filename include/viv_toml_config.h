#ifndef VIV_TOML_CONFIG_H
#define VIV_TOML_CONFIG_H

#include <unistd.h>

#include "viv_types.h"

void viv_toml_config_load(char *path, struct viv_config *config, bool user_path);

#endif
