viv_sources = [
  'vivarium.c',
  'viv_background.c',
  'viv_bar.c',
  'viv_cursor.c',
  'viv_ipc.c',
  'viv_layout.c',
  'viv_mappable_functions.c',
  'viv_output.c',
  'viv_render.c',
  'viv_server.c',
  'viv_layer_view.c',
  'viv_view.c',
  'viv_wl_list_utils.c',
  'viv_workspace.c',
  'viv_xdg_shell.c',
  'viv_xwayland_shell.c',
]

viv_deps = [
    wlroots_dep,
    wayland_server_dep,
    xkbcommon_dep,
    xdg_shell_protocol_dep,
    layer_shell_protocol_dep,
]

executable(
  'vivarium',
  viv_sources,
  include_directories : includes,
  dependencies : viv_deps,
)
