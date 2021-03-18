#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>
#include <wlr/util/log.h>

static void run_swaybg(char *colour, char *image, char *mode) {
	pid_t pid = fork();
	if (pid == 0) {
        // TODO: We need to do more shenanigans to fork properly/safely, see e.g. sway's version

        wlr_log(WLR_INFO, "Running swaybg");

        (void)colour;
        char *const cmd[] = {
            "swaybg",
            "-c", colour,
            "-i", image,
            "-m", mode,
            NULL
        };
        execvp(cmd[0], cmd);
        _exit(EXIT_SUCCESS);
    }
}

void viv_parse_and_run_background_config(char *colour, char *image, char *mode) {
    if (!strlen(colour) && !strlen(image) && !strlen(mode)) {
        // Nothing is configured so just don't run swaybg
        wlr_log(WLR_ERROR, "No background config, skipping");
        return;
    }

    run_swaybg(colour, image, mode);
}
