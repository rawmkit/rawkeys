project(
  'rawkeys',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: ['c_std=c99', 'warning_level=3'],
  meson_version: '>=1.6.0',
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

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
