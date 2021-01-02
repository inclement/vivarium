#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>


void viv_run_background_process(char *colour, char *image, char *mode) {
	pid_t pid = fork();
	if (pid == 0) {
        // TODO: We need to do more shenanigans to fork properly/safely, see e.g. sway's version

        char *const cmd[] = {
            "swaybg",
            "-c", colour,
            "-i", image,
            "-m", mode,
            NULL};
        execvp(cmd[0], cmd);
        _exit(EXIT_FAILURE);
    }
}
