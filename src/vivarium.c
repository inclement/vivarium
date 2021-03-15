#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "viv_toml_config.h"
#include "viv_types.h"
#include "viv_server.h"

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

	wlr_log_init(WLR_DEBUG, NULL);

    // Initialise our vivarium server. This sets up all the event bindings so that inputs,
    // outputs and window events can be handles.
	struct viv_server server = { .config = NULL };
    viv_server_init(&server);

	// Add a Unix socket to the Wayland display.
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

    // Start the wlroots backend, which will manage outputs/inputs, becaome the DRM master
    // etc.
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

    // Set the WAYLAND_DISPLAY environment variable, so that clients know how to connect
    // to our server
	setenv("WAYLAND_DISPLAY", socket, true);

    // Set up env vars to encourage applications to use wayland if possible
    // TODO: Should this be necessary? Or is there a better way to do it, for wayland
    // using an X11 backend?
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("GDK_BACKEND", "wayland", true);  // NOTE: not a typo, it really is GDK not GTK

    // Start the wayland eventloop. From here, all compositor activity comes via events it
    // sends us.
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(server.wl_display);

    viv_server_deinit(&server);

	return 0;
}
