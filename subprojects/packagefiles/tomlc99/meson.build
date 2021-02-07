project(
	'toml',
	'c',
    default_options : [
      'c_std=c99',
    ]
)

cc = meson.get_compiler('c')

include_dir = include_directories('.')

lib_tomlc99 = library(
  meson.project_name(),
  ['toml.c'],
  include_directories: [include_dir],
)

tomlc99_dep = declare_dependency(
  link_with: lib_tomlc99,
  include_directories: [include_dir],
)
