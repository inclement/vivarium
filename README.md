Vivarium
========

A dynamic tiling [Wayland](https://wayland.freedesktop.org/) compositor using [wlroots](https://github.com/swaywm/wlroots), with desktop semantics inspired by [xmonad](https://xmonad.org/).

Core features include:

* Automatic/dynamic tiling with your choice of layouts.
* Per-output workspaces: display the workspace you want, where you want.
* Floating windows on demand.
* (optional) XWayland support.
* Wayland layer shell support, compatible with tools like [Waybar](https://github.com/Alexays/Waybar) and [swaybg](https://github.com/swaywm/swaybg).

Vivarium is unstable and unfinished...but usable!

Build instructions
------------------

Get Vivarium:

    git clone git@github.com:inclement/vivarium.git
    cd vivarium

Build Vivarium:

    meson build
    ninja -C build

Run Vivarium:

    ./build/src/vivarium

Vivarium expects to be run from a TTY, but also supports embedding in an X session or existing Wayland session out of the box. Running the binary will Do The Right Thing.

Configuration
-------------

Vivarium is currently configured via a configuration struct definde in `config/viv_config.h`: edit that file before building to update its behaviour.

This configuration method is unstable and expected to change significantly, but for now it keeps things simple in order to focus on developing the main body of the compositor.

Configuration options include but are not limited to:

* Custom hotkeys for all types of window manipulations.
* Selection of layouts and their properties.
* Customise window borders.
* Mouse interaction options.
* Keyboard layout selection.
