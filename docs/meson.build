gidocgen_dep = dependency('gi-docgen',
  version: '>= 2021.1',
  native: true,
  required: false,
)
gidocgen_app = find_program('gi-docgen')

if gidocgen_dep.found() and gidocgen_app.found() and get_option('introspection')
  toml_conf = configuration_data()
  toml_conf.set('version', meson.project_version())

  libgusb_toml = configure_file(
    input: 'libgusb.toml.in',
    output: 'libgusb.toml',
    configuration: toml_conf
  )

  custom_target('doc-libgusb',
    input: [
      libgusb_toml,
      libgusb_girtarget[0],
    ],
    output: 'libgusb',
    command: [
      gidocgen_app,
      'generate',
      '--quiet',
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../libgusb'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depends: [
      libgusb_girtarget[0],
    ],
    build_by_default: true,
    install: true,
    install_dir: join_paths(datadir, 'doc'),
  )
  install_data(['urlmap_libgusb.js'],
    install_dir: join_paths(datadir, 'doc', 'libgusb')
  )
endif
