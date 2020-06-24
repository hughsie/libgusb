gusb_version_xml = configure_file(
  input : 'version.xml.in',
  output : 'version.xml',
  configuration : conf,
)

gnome.gtkdoc(
  'gusb',
  src_dir : [
    gusb_source_dir,
    gusb_build_dir,
  ],
  main_sgml : 'gusb-docs.sgml',
  install : true
)
