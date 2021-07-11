project(
    'vivarium',
    'c',
    version : '0.0.4dev',
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

if get_option('headless-test')
  add_project_arguments([
    '-DHEADLESS_TEST',
    ], language : 'c')
endif

cc = meson.get_compiler('c')

# For wlroots, use the system version if the correct version is available
wlroots_dep = dependency('wlroots', version : ['>=0.15.0'], fallback : ['wlroots', 'wlroots'],
                         default_options : ['examples=false'])

tomlc99_dep = dependency('toml', fallback : ['tomlc99', 'tomlc99_static_dep'])

# Unit testing deps
fff_dep = dependency('fff', fallback : ['fff', 'fff_dep'])
unity_dep = dependency('unity', fallback : ['unity', 'unity_dep'])

wayland_server_dep = dependency('wayland-server')
wayland_client_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')
xkbcommon_dep = dependency('xkbcommon')
libinput_dep = dependency('libinput')
xcb_dep = dependency('xcb', required: get_option('xwayland'))
pixman_dep = dependency('pixman-1')

math_dep = cc.find_library('m')

includes = [
  include_directories('include'),
  include_directories('protocols'),
  include_directories(get_option('config-dir')),
]

subdir('include')
subdir('protocols')
subdir('src')
subdir('tests')

# Make sure our default config file is also installed so users can copy it into place.
# We don't actually need the meson config file declaration for this, but it's convenient.
conf_data = configuration_data()
configure_file(input : 'config/config.toml',
               output : 'config.toml',
               install_dir : join_paths(get_option('sysconfdir'), 'vivarium'),
               configuration : conf_data)
