
project(
	'fff',
	'c',
    default_options : [
      'c_std=c99',
    ]
)

cc = meson.get_compiler('c')

include_dir = include_directories('.')

fff_dep = declare_dependency(
  include_directories: [include_dir],
)
