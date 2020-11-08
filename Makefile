PROTOCOLS_DIR=./protocols

IDIR = include
SDIR = src
ODIR = obj

EXTRA_INCLUDES = -I/usr/include/pixman-1

CFLAGS += \
	-DWLR_USE_UNSTABLE \
	-I$(IDIR) \
	-Iprotocols \
	-g \
	-Werror \
	$(EXTRA_INCLUDES)


_PROTOCOLS = xdg-shell
PROTOCOL_INCLUDES = $(patsubst %,$(PROTOCOLS_DIR)/%-protocol.h,$(_PROTOCOLS))
PROTOCOL_SOURCES = $(patsubst %,$(PROTOCOLS_DIR)/%-protocol.c,$(_PROTOCOLS))

_DEPS = viv_types.h viv_server.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS)) $(PROTOCOL_INCLUDES) $(PROTOCOL_SOURCES)

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

LIBS=\
	 $(shell pkg-config --cflags --libs wlroots) \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon)

_OBJ = viv_server.o vivarium.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

$(PROTOCOLS_DIR)/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

$(PROTOCOLS_DIR)/xdg-shell-protocol.c: $(PROTOCOLS_DIR)/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	mkdir -p $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

vivarium: $(OBJ)
	$(CC) $(CFLAGS) \
		-o $@ $^ \
		$(LIBS)

run: vivarium
	./vivarium

clean:
	rm -f vivarium $(PROTOCOLS_DIR)/* $(ODIR)/*

.DEFAULT_GOAL=vivarium
.PHONY: clean
