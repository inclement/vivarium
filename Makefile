PROTOCOLS_DIR=./protocols

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=\
	 $(shell pkg-config --cflags --libs wlroots) \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon)

$(PROTOCOLS_DIR)/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

$(PROTOCOLS_DIR)/xdg-shell-protocol.c: $(PROTOCOLS_DIR)/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

vivarium: src/vivarium.c $(PROTOCOLS_DIR)/xdg-shell-protocol.h $(PROTOCOLS_DIR)/xdg-shell-protocol.c
	$(CC) $(CFLAGS) \
		-g -Werror -Iinclude -Iprotocols \
		-DWLR_USE_UNSTABLE \
		-o $@ $< \
		$(LIBS)

clean:
	rm -f vivarium xdg-shell-protocol.h xdg-shell-protocol.c

.DEFAULT_GOAL=vivarium
.PHONY: clean
