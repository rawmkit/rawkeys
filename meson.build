project(
  'rawkeys',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=c99',
    'warning_level=3',
    'sysconfdir=/etc',
  ],
  meson_version: '>=1.6.0',
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

sysconfdir = get_option('sysconfdir')
runtime_sysconfdir = sysconfdir
if not runtime_sysconfdir.startswith('/')
  runtime_sysconfdir = get_option('prefix') / runtime_sysconfdir
endif
add_project_arguments(
  '-DSYSCONFDIR="@0@"'.format(runtime_sysconfdir),
  language: 'c',
)

cc = meson.get_compiler('c')
x11 = dependency('x11')
libconfig = dependency('libconfig')

install_data('LICENSE', install_dir: get_option('datadir') / 'licenses' / 'rawkeys')
install_data('README.md', 'NEWS.md', 'PROVENANCE.md',
             install_dir: get_option('datadir') / 'doc' / 'rawkeys')

subdir('src')
subdir('config')
subdir('docs')
subdir('tests')
