test_deps = [
  fff_dep,
  unity_dep,
]

config_test_sources = [
  'test_config.c',
  '../src/viv_toml_config.c',
]

config_test = executable(
  'config-test',
  config_test_sources,
  include_directories : includes + ['./'],
  dependencies : viv_deps + test_deps,
)

test('config test', config_test)
