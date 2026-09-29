scdoc = find_program('scdoc', required: true)
foreach page : ['rawkeys.1', 'rawkeys.conf.5']
  generated = custom_target(
    page,
    input: page + '.scdoc',
    output: page,
    command: [scdoc],
    feed: true,
    capture: true,
    install: true,
    install_dir: get_option('mandir') / (page.endswith('.1') ? 'man1' : 'man5'),
  )
endforeach
