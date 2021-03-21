test_deps = [
  fff_dep,
  unity_dep,
]

config_test_sources = [
  'config_test.c'
]

config_test = executable(
  'config-test',
  config_test_sources,
  include_directories : includes,
  dependencies : viv_deps + test_deps,
)

test('config test', config_test)
