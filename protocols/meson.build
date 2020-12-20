
wayland_protocols_dir = wayland_protocols_dep.get_pkgconfig_variable('pkgdatadir')
local_protocols_dir = 'xml'

wayland_scanner = find_program('wayland-scanner', native : true)


xdg_shell_src = custom_target(
  'xdg_shell_protocol_c',
  input : join_paths([wayland_protocols_dir, 'stable/xdg-shell/xdg-shell.xml']),
  output : '@BASENAME@-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_include = custom_target(
  'xdg_shell_protocol_h',
  input : join_paths([wayland_protocols_dir, 'stable/xdg-shell/xdg-shell.xml']),
  output : '@BASENAME@-protocol.h',
  command : [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_protocol = static_library(
  'xdg_shell_protocol',
  [xdg_shell_src, xdg_shell_include],
)

xdg_shell_protocol_dep = declare_dependency(
  link_with : xdg_shell_protocol,
  sources : xdg_shell_include,
)


layer_shell_src = custom_target(
  'layer_shell_protocol_c',
  input : join_paths([local_protocols_dir, 'wlr-layer-shell-unstable-v1.xml']),
  output : '@BASENAME@-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

layer_shell_include = custom_target(
  'layer_shell_protocol_h',
  input : join_paths([local_protocols_dir, 'wlr-layer-shell-unstable-v1.xml']),
  output : '@BASENAME@-protocol.h',
  command : [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)

layer_shell_protocol = static_library(
  'layer_shell_protocol',
  [layer_shell_src, layer_shell_include],
)

layer_shell_protocol_dep = declare_dependency(
  link_with : layer_shell_protocol,
  sources : layer_shell_include,
)
