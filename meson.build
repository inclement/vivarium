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
    '-DWLR_USE_UNSTABLE',
    '-D_POSIX_C_SOURCE=200809',
], language : 'c')

if get_option('develop')
  add_project_arguments([
    '-DDEBUG',
  ], language : 'c')
endif

if get_option('xwayland').enabled()
  add_project_arguments([
    '-DXWAYLAND',
  ], language : 'c')
endif



cc = meson.get_compiler('c')

# For wlroots, use the system version if available
wlroots_dep = dependency('wlroots', version : '>=0.12', fallback : ['wlroots', 'wlroots'])

wayland_server_dep = dependency('wayland-server')
wayland_client_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')


includes = [
  include_directories('include'),
  include_directories('protocols'),
  include_directories(get_option('config-dir')),
]

subdir('include')
subdir('protocols')
subdir('src')
