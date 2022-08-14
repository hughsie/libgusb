cargs = ['-DG_LOG_DOMAIN="GUsb"']

executable(
  'gusbcmd',
  sources : [
    'gusb-main.c',
  ],
  include_directories : [
      lib_incdir,
      root_incdir,
  ],
  dependencies : [
    libgio,
    libusb,
    libjsonglib,
  ],
  link_with : gusb,
  c_args : [
    cargs,
  ],
  install : true,
  install_dir : bindir
)
