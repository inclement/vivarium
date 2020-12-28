project(
    'vivarium',
    'c',
    version : '0.0.1',
    default_options : [
      'c_std=c11',
      'warning_level=2',
      'werror=true',
    ],
)

add_project_arguments([
    '-DDEBUG',  # TODO: make this a build option
    '-DXWAYLAND',  # TODO: make this a build option
    '-DWLR_USE_UNSTABLE',
    '-D_POSIX_C_SOURCE=200809',
], language : 'c')

cc = meson.get_compiler('c')

wlroots_dep = dependency('wlroots', version : '>=0.12')
wayland_server_dep = dependency('wayland-server')
wayland_client_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')
xkbcommon_dep = dependency('xkbcommon')


includes = [
  include_directories('include'),
  include_directories('protocols'),
  include_directories('config'),
]

subdir('include')
subdir('protocols')
subdir('src')
