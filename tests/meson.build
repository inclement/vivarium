test_deps = [
  fff_dep,
  unity_dep,
]

test_config = executable(
  'test-config',
  ['test_config.c', '../src/viv_toml_config.c'],
  include_directories : includes + ['./'],
  dependencies : viv_deps + test_deps,
)

test_layouts = executable(
  'test-layouts',
  ['test_layouts.c', '../src/viv_layout.c'],
  include_directories : includes + ['./'],
  dependencies : viv_deps + test_deps,
)

test('Test config', test_config)
test('Test layouts', test_layouts)
