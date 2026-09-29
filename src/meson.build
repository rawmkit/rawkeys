rawkeys_sources = files(
  'main.c',
  'config.c',
  'rawm.c',
  'rawpanel.c',
  'xkb.c',
)

rawkeys_exe = executable(
  'rawkeys',
  rawkeys_sources,
  dependencies: [x11, libconfig],
  c_args: ['-DVERSION="@0@"'.format(meson.project_version())],
  install: true,
)
