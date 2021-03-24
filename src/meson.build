viv_sources = [
  'vivarium.c',
  'viv_background.c',
  'viv_bar.c',
  'viv_cli.c',
  'viv_cursor.c',
  'viv_input.c',
  'viv_ipc.c',
  'viv_layout.c',
  'viv_mappable_functions.c',
  'viv_output.c',
  'viv_render.c',
  'viv_server.c',
  'viv_layer_view.c',
  'viv_toml_config.c',
  'viv_view.c',
  'viv_wl_list_utils.c',
  'viv_workspace.c',
  'viv_xdg_shell.c',
]

if get_option('xwayland').enabled()
  viv_sources += [
    'viv_xwayland_shell.c',
  ]
endif

viv_deps = [
    wlroots_dep,
    wayland_server_dep,
    xkbcommon_dep,
    libinput_dep,
    xdg_shell_protocol_dep,
    layer_shell_protocol_dep,
    tomlc99_dep,
]

executable(
  'vivarium',
  viv_sources,
  include_directories : includes,
  dependencies : viv_deps,
  install : true,
)
