display_probe = executable(
  'display-probe',
  'display-probe.c',
  dependencies: x11,
)

fake_authority = executable(
  'fake-authority',
  'fake-authority.c',
  dependencies: x11,
)
selection_owner = executable(
  'selection-owner',
  'selection-owner.c',
  dependencies: x11,
)

dl_dep = cc.find_library('dl', required: false)
send_key_deps = [x11]
if dl_dep.found()
  send_key_deps += dl_dep
endif
send_key = executable(
  'send-key',
  'send-key.c',
  dependencies: send_key_deps,
)

test(
  'config',
  find_program('config.sh'),
  args: [rawkeys_exe, meson.project_source_root() / 'config' / 'rawkeys.conf'],
)

config_defaults_test = executable(
  'config-defaults-test',
  'config-defaults-test.c',
  '../src/config.c',
  include_directories: include_directories('../src'),
  dependencies: libconfig,
)

test(
  'config-defaults',
  config_defaults_test,
  args: [meson.project_source_root() / 'config' / 'rawkeys.conf'],
)

test(
  'routing',
  find_program('routing.sh'),
  args: [
    rawkeys_exe,
    display_probe,
    fake_authority,
    selection_owner,
    send_key,
    meson.current_source_dir() / 'exec-probe.sh',
    meson.current_source_dir() / 'capture-probe.sh',
  ],
  timeout: 30,
)

xkb_test = executable(
  'xkb-test',
  'xkb-test.c',
  '../src/xkb.c',
  include_directories: include_directories('../src'),
  dependencies: x11,
)

test(
  'xkb',
  find_program('xkb.sh'),
  args: [xkb_test, display_probe],
  timeout: 15,
)

rawm_clients_test = executable(
  'rawm-clients-test',
  'rawm-clients-test.c',
  '../src/rawm.c',
  include_directories: include_directories('../src'),
  dependencies: x11,
)

test(
  'rawm-clients',
  find_program('rawm-clients.sh'),
  args: [rawm_clients_test, display_probe],
  timeout: 15,
)
