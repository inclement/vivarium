#ifndef VIV_CLI_H
#define VIV_CLI_H

#include "viv_types.h"

struct viv_args {
    char *config_filen;
};

struct viv_args viv_cli_parse_args(int argc, char *argv[]);

#endif
