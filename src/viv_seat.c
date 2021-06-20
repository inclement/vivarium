#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>

#include "viv_config_support.h"
#include "viv_seat.h"
#include "viv_types.h"

/// Handle a cursor request (i.e. client requesting a specific image)
static void seat_request_cursor(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, request_cursor);
    struct viv_server *server = seat->server;
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = seat->wlr_seat->pointer_state.focused_client;

    // Ignore the request if the client isn't currently pointer-focused
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

/// Handle a request to set the selection
static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct viv_seat *seat = wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->wlr_seat, event->source, event->serial);
}

struct viv_seat *viv_seat_create(struct viv_server *server, char *name) {
    struct wl_display *wl_display = server->wl_display;

    struct viv_seat *seat = calloc(1, sizeof(struct viv_seat));
    CHECK_ALLOCATION(seat);

    seat->server = server;

    struct wlr_seat *wlr_seat = wlr_seat_create(wl_display, name);
    CHECK_ALLOCATION(wlr_seat);
    seat->wlr_seat = wlr_seat;

    wl_list_init(&seat->server_link);
    wl_list_insert(server->seats.prev, &seat->server_link);


	seat->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&wlr_seat->events.request_set_cursor, &seat->request_cursor);
	seat->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&wlr_seat->events.request_set_selection, &seat->request_set_selection);

    return seat;
}
