# Vivarium

A dynamic tiling [Wayland](https://wayland.freedesktop.org/) compositor using [wlroots](https://github.com/swaywm/wlroots), with desktop semantics inspired by [xmonad](https://xmonad.org/).

<p align="center">
  <img src="media/readme_screenshot.png" width="80%" alt="Vivarium screenshot showing several tiled windows">
</p>

Core features include:

* Automatic/dynamic tiling with your choice of layouts.
* Per-output workspaces: display the workspace you want, where you want.
* Floating windows on demand.
* (optional) XWayland support.
* Layer shell support, compatible with tools like [Waybar](https://github.com/Alexays/Waybar) and [swaybg](https://github.com/swaywm/swaybg).

Vivarium is unstable and unfinished...but usable!

## Build instructions

Get install dependencies. You need:

* meson
* wlroots
* wayland
* wayland-protocols

Get Vivarium:

    git clone git@github.com:inclement/vivarium.git
    cd vivarium

Build Vivarium:

    meson build
    ninja -C build

Run Vivarium:

    ./build/src/vivarium

(optional) Install Vivarium:

    sudo ninja -C build install

Vivarium expects to be run from a TTY, but also supports embedding in an X session or existing Wayland session out of the box. Running the binary will Do The Right Thing.

## Configuration


Vivarium is currently configured via a configuration struct defined in `config/viv_config.h`: edit that file before building to update its behaviour.

This configuration method is unstable and expected to change significantly, but for now it keeps things simple in order to focus on developing the main body of the compositor.

Configuration options include but are not limited to:

* Custom hotkeys for all types of window manipulations.
* Choose the list of layouts to use.
* Window borders.
* Keyboard layout(s).
* Status bar configuration.
* Background image/colour.
* Mouse interaction options.

### Bar support

Vivarium can automatically start a bar program such as [Waybar](https://github.com/Alexays/Waybar). Only Waybar is currently tested, and only very basic IPC is currently possible, but this is enough to display the current workspace status.

See `viv_config.h` for instructions, but in summary you'll need the following configuration:

    // Choose a filename at which Vivarium will write a workspace status string
    .ipc_workspaces_filename = "/path/to/status/file",

    // Configure bar autostart
    .bar = {
        .command = "waybar",
        .update_signal_number = 1,  // If non-zero, Vivarium sends the bar process
                                    // SIGRTMIN + update_signal_number on changes
    },

Then in your Waybar config:

    "modules-left": ["custom/workspaces"],

    "custom/workspaces": {
        "exec": "cat /path/to/status/file",
        "interval": "once",
        "signal": 1,
        "format": " {} ",
        "escape": true,
    },

Note that the `"signal"` option matches the `update_signal_number` from Vivarium's config, telling Waybar to refresh (re-read the status file) when the signal is received.

## FAQ (or not-so-FAQ)

> What does "desktop semantics inspired by xmonad" mean?

The core tiling experience provides something similar to xmonad's defaults: new windows are added to the current workspace and tiled automatically within a current layout. Each workspace may independently switch between different layouts. Each output (usually equivalent to each monitor) displays a single workspace, and each may be switched independently.

Vivarium makes no attempt to rigorously mimic xmonad or to replicate its internal design philosophy. Not least, Vivarium is written in C and is not (for now) so directly and transparently extensible.

> Why do some windows display title bars with maximize/minimize/close buttons that don't do anything?
> Can I turn that off?

Vivarium attempts to tell windows not to draw their own decorations, but the protocols for doing so are not yet standard or universally supported so some windows still do so. For now there's probably nothing you can do about it, but this is likely to improve in the future.

(It's also possible that there are bugs in Vivarium's window decoration configuration, bug reports welcome if so.)

> Does Vivarium support $PROTOCOL? Will it in the future?

I'm aiming to support all the core wayland protocols plus all the extra ones being developed under wlroots. However, there is no ETA for specific protocols right now.

Currently supported protocols (though all may be incomplete or buggy in places, none are battle tested):

* XDG shell
* XDG output
* XDG decoration
* XWayland
* Layer shell

> Can you add $FEATURE?

I'm not sure! At the time of writing Vivarium is a personal project whose design philosophy isn't fully determined. Suggestions and requests are welcome.
