gusb_version_xml = configure_file(
  input : 'version.xml.in',
  output : 'version.xml',
  configuration : conf,
)

gnome.gtkdoc(
  'gusb',
  src_dir : [
    join_paths(meson.source_root(), 'gusb'),
    join_paths(meson.build_root(), 'gusb'),
  ],
  main_sgml : 'gusb-docs.sgml',
  install : true
)
