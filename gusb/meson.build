cargs = [
  '-DG_LOG_DOMAIN="libgusb"',
  '-DUSB_IDS="' + get_option('usb_ids') + '"',
]

con2 = configuration_data()
con2.set('G_USB_MAJOR_VERSION', gusb_major_version)
con2.set('G_USB_MINOR_VERSION', gusb_minor_version)
con2.set('G_USB_MICRO_VERSION', gusb_micro_version)

gusb_version_h = configure_file(
  input : 'gusb-version.h.in',
  output : 'gusb-version.h',
  configuration : con2
)

install_headers(
  'gusb.h',
  subdir : 'gusb-1',
)

lib_incdir = include_directories('.')

install_headers([
    gusb_version_h,
    'gusb-autocleanups.h',
    'gusb-context.h',
    'gusb-context-private.h',
    'gusb-device.h',
    'gusb-device-private.h',
    'gusb-device-list.h',
    'gusb-interface.h',
    'gusb-interface-private.h',
    'gusb-source.h',
    'gusb-util.h',
  ],
  subdir : 'gusb-1/gusb',
)

mapfile = 'libgusb.ver'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
gusb = shared_library(
  'gusb',
  sources : [
    'gusb-context.c',
    'gusb-device.c',
    'gusb-device-list.c',
    'gusb-interface.c',
    'gusb-source.c',
    'gusb-util.c',
    'gusb-version.c',
  ],
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    libgio,
    libusb,
  ],
  c_args : [
      cargs,
    ],
  include_directories : [
      root_incdir,
      lib_incdir,
  ],
  link_args : vflag,
  link_depends : mapfile,
  install : true
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : gusb,
  requires : [ 'gio-2.0', 'gobject-2.0', 'libusb-1.0' ],
  subdirs : 'gusb-1',
  version : meson.project_version(),
  name : 'gusb',
  filebase : 'gusb',
  description : 'GUsb is a GLib binding for libusb1',
)

libgusb_girtarget = gnome.generate_gir(gusb,
  sources : [
    gusb_version_h,
    'gusb-autocleanups.h',
    'gusb-context.c',
    'gusb-context.h',
    'gusb-context-private.h',
    'gusb-device.c',
    'gusb-device.h',
    'gusb-device-list.c',
    'gusb-device-list.h',
    'gusb-device-private.h',
    'gusb-interface.c',
    'gusb-interface.h',
    'gusb-interface-private.h',
    'gusb-source.c',
    'gusb-source.h',
    'gusb-util.c',
    'gusb-util.h',
  ],
  nsversion : '1.0',
  namespace : 'GUsb',
  symbol_prefix : 'g_usb',
  identifier_prefix : 'GUsb',
  export_packages : 'gusb',
  extra_args : [
    '--c-include=gusb.h',
    ],
  link_with : gusb,
  dependencies : [
    libgio,
    libusb,
  ],
  includes : [
    'Gio-2.0',
    'GObject-2.0',
  ],
  install : true
)
libgusb_gir = libgusb_girtarget[0]
libgusb_typelib = libgusb_girtarget[1]

if get_option('vapi')
gnome.generate_vapi('gusb',
  sources: libgusb_girtarget[0],
  packages: ['gio-2.0'],
  install: true,
)
endif

if get_option('tests')
  e = executable(
    'gusb-self-test',
    sources : [
      'gusb-context.c',
      'gusb-device.c',
      'gusb-device-list.c',
      'gusb-interface.c',
      'gusb-self-test.c',
      'gusb-source.c',
      'gusb-util.c',
    ],
    include_directories : [
      root_incdir,
      lib_incdir,
    ],
    dependencies : [
      libgio,
      libusb,
    ],
    c_args : [
      cargs,
    ],
  )
  test('gusb-self-test', e)
endif
