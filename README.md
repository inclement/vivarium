# Vivarium

![Build Status](https://github.com/inclement/vivarium/workflows/Build%20Vivarium/badge.svg)

A dynamic tiling [Wayland](https://wayland.freedesktop.org/) compositor using [wlroots](https://github.com/swaywm/wlroots), with desktop semantics inspired by [xmonad](https://xmonad.org/).

<p align="center">
  <img src="media/readme_screenshot.png" width="80%" alt="Vivarium screenshot showing several tiled windows">
</p>

Core features include:

* Automatic/dynamic tiling with your choice of layouts.
* Per-output workspaces: display the workspace you want, where you want.
* Floating windows on demand.
* (optional) XWayland support.
* Layer shell support, compatible with tools like [Waybar](https://github.com/Alexays/Waybar), [bemenu](https://github.com/Cloudef/bemenu) and [swaybg](https://github.com/swaywm/swaybg).

Vivarium is unstable and unfinished...but usable!

## Tiling model

Vivarium lets you define any number of workspaces, each with some number of tiling layouts that you can switch between at runtime. New windows are automatically tiled according to the layout, or can be made floating to be placed anywhere with any size you like. The order of windows within the layout is adjustable at runtime.

You will probably want to set up a small number of layouts, updating their parameters according to your needs. For instance, if you find you need too many terminals to fit in a single stack next to a browser window then you might switch the layout to one that places windows in multiple columns. Or if you want to focus on the browser, you might switch to a fullscreen layout that displays only the active window.

Example layouts include (left to right): split, fullscreen, central column, and recursive split:

<p align="center">
  <img src="media/layout_type_illustrations.png" alt="Illustrated Vivarium layouts">
</p>

Most layouts have a main panel displaying the largest window, and a secondary space for the other windows.

Layouts have a "fill fraction" parameter, adjustable at runtime via hotkeys, which controls the size of the main panel:

<p align="center">
  <img src="media/layout_split_dist_illustrations.png" alt="Illustrated Vivarium layouts with different fill fraction">
</p>

Layouts also have an integer main panel "count", adjustable at runtime via hotkeys, which controls how many windows are stacked in the main panel. It can be zero so that all windows occupy the secondary space:

<p align="center">
  <img src="media/layout_counter_illustrations.png" alt="Illustrated Vivarium layouts with different main panel counts">
</p>


Other per-layout options include whether window borders are displayed, and whether the layout leaves space for programs like the desktop bar or draws windows over their normally-excluded region.


## Build instructions

Get install dependencies. You need:

* meson
* wlroots
* wayland
* wayland-protocols
* xcb

Specific package dependencies for Ubuntu 20.04 can be found in [the Github CI file](.github/workflows/main.yml).

Get Vivarium:

    git clone git@github.com:inclement/vivarium.git
    cd vivarium

Build Vivarium:

    meson build
    ninja -C build

Run Vivarium:

    ./build/src/vivarium

Install Vivarium:

    sudo ninja -C build install

Vivarium expects to be run from a TTY, but also supports embedding in an X session or existing Wayland session out of the box. Running the binary will Do The Right Thing.

## Configuration & FAQ

See our [wiki](https://github.com/inclement/vivarium/wiki).

