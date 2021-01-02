#ifndef VIV_BAR_H
#define VIV_BAR_H

#include <signal.h>
#include <stdint.h>

/// Fork and run swaybg with the given options
pid_t viv_parse_and_run_bar_config(char *bar_command, uint32_t update_signal_number);

#endif
