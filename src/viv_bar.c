#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "viv_config_support.h"

static pid_t run_bar(char *bar_command) {
	pid_t pid = fork();
	if (pid == 0) {
        // TODO: We need to do more shenanigans to fork properly/safely, see e.g. sway's version

        wlr_log(WLR_INFO, "Running swaybg");

        char *const cmd[] = {
            bar_command,
            NULL
        };
        execvp(cmd[0], cmd);
        _exit(EXIT_FAILURE);
    }
    return pid;
}

pid_t viv_parse_and_run_bar_config(char *bar_command, uint32_t update_signal_number) {
    if (!strlen(bar_command)) {
        // Nothing is configured so just don't run swaybg
        wlr_log(WLR_INFO, "No bar config, skipping");
        return (pid_t)0;
    }

    ASSERT((int)update_signal_number <= (SIGRTMAX - SIGRTMIN));

    return run_bar(bar_command);
}
