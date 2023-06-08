cargs = [
  '-DG_LOG_DOMAIN="libgusb"',
  '-DUSB_IDS="' + get_option('usb_ids') + '"',
]

gusb_source_dir = meson.current_source_dir()
gusb_build_dir = meson.current_build_dir()

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
    'gusb-context.h',
    'gusb-context-private.h',
    'gusb-device.h',
    'gusb-device-event.h',
    'gusb-device-private.h',
    'gusb-device-list.h',
    'gusb-interface.h',
    'gusb-interface-private.h',
    'gusb-bos-descriptor.h',
    'gusb-bos-descriptor-private.h',
    'gusb-endpoint.h',
    'gusb-endpoint-private.h',
    'gusb-source.h',
    'gusb-util.h',
  ],
  subdir : 'gusb-1/gusb',
)

mapfile = 'libgusb.ver'
mapfile_test = 'libgusb_meson.ver'
vflag = []
vflag_test = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile_test)
if cc.has_link_argument(vflag_test)
  vflag += '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
endif
gusb = library(
  'gusb',
  sources : [
    'gusb-context.c',
    'gusb-device.c',
    'gusb-device-event.c',
    'gusb-device-list.c',
    'gusb-interface.c',
    'gusb-bos-descriptor.c',
    'gusb-endpoint.c',
    'gusb-source.c',
    'gusb-util.c',
    'gusb-version.c',
    'gusb-json-common.c',
  ],
  soversion : lt_current,
  version : lt_version,
  darwin_versions: [lt_current + 1, '@0@.@1@.0'.format(lt_current + 1, lt_revision)],
  dependencies : [
    libgio,
    libusb,
    libjsonglib,
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

gusb_dep = declare_dependency(
  link_with : gusb,
  include_directories : [
      root_incdir,
      lib_incdir,
  ],
  dependencies : gusb_deps
)

pkgg = import('pkgconfig')
pkgg.generate(gusb,
  requires : [ 'gio-2.0', 'gobject-2.0', 'libusb-1.0', 'json-glib-1.0' ],
  subdirs : 'gusb-1',
  version : meson.project_version(),
  name : 'gusb',
  filebase : 'gusb',
  description : 'GUsb is a GLib binding for libusb1',
)

if get_option('introspection')
libgusb_girtarget = gnome.generate_gir(gusb,
  sources : [
    gusb_version_h,
    'gusb-context.c',
    'gusb-context.h',
    'gusb-context-private.h',
    'gusb-device.c',
    'gusb-device.h',
    'gusb-device-event.c',
    'gusb-device-event.h',
    'gusb-device-event-private.h',
    'gusb-device-list.c',
    'gusb-device-list.h',
    'gusb-device-private.h',
    'gusb-interface.c',
    'gusb-interface.h',
    'gusb-interface-private.h',
    'gusb-bos-descriptor.c',
    'gusb-bos-descriptor.h',
    'gusb-bos-descriptor-private.h',
    'gusb-endpoint.c',
    'gusb-endpoint.h',
    'gusb-endpoint-private.h',
    'gusb-source.c',
    'gusb-source.h',
    'gusb-util.c',
    'gusb-util.h',
    'gusb-version.c',
  ],
  nsversion : '1.0',
  namespace : 'GUsb',
  symbol_prefix : 'g_usb',
  identifier_prefix : 'GUsb',
  export_packages : 'gusb',
  extra_args : [
    '--c-include=gusb.h',
    '-DGUSB_COMPILATION',
    ],
  link_with : gusb,
  dependencies : [
    libgio,
    libusb,
    libjsonglib,
  ],
  includes : [
    'Gio-2.0',
    'GObject-2.0',
    'Json-1.0',
  ],
  install : true
)
libgusb_gir = libgusb_girtarget[0]
libgusb_typelib = libgusb_girtarget[1]

pymod = import('python')
py_installation = pymod.find_installation()

# Verify the map file is correct -- note we can't actually use the generated
# file for two reasons:
#
#  1. We don't hard depend on GObject Introspection
#  2. The map file is required to build the lib that the GIR is built from
#
# To avoid the circular dep, and to ensure we don't change exported API
# accidentally actually check in a version of the version script to git.
mapfile_target = custom_target('gusb_mapfile',
  input: libgusb_girtarget[0],
  output: 'libgusb.ver',
  command: [
    py_installation,
    join_paths(meson.project_source_root(), 'contrib', 'generate-version-script.py'),
    'LIBGUSB',
    '@INPUT@',
    '@OUTPUT@',
  ],
)
diffcmd = find_program('diff')
test('gusb-exported-api', diffcmd,
     args : [
      '-urNp',
      join_paths(meson.current_source_dir(), 'libgusb.ver'),
      mapfile_target,
      ],
    )

endif

if get_option('vapi')
gnome.generate_vapi('gusb',
  sources: libgusb_girtarget[0],
  packages: ['gio-2.0', 'json-glib-1.0'],
  install: true,
)
endif

if get_option('tests')
  e = executable(
    'gusb-self-test',
    sources : [
      'gusb-context.c',
      'gusb-device.c',
      'gusb-device-event.c',
      'gusb-device-list.c',
      'gusb-interface.c',
      'gusb-bos-descriptor.c',
      'gusb-endpoint.c',
      'gusb-self-test.c',
      'gusb-source.c',
      'gusb-util.c',
      'gusb-json-common.c'
    ],
    include_directories : [
      root_incdir,
      lib_incdir,
    ],
    dependencies : [
      libgio,
      libusb,
      libjsonglib,
    ],
    c_args : [
      cargs,
    ],
  )
  test('gusb-self-test', e)

  # Umockdev based tests
  test_env = environment()
  test_env.prepend('LD_PRELOAD', 'libumockdev-preload.so.0')

  test_umockdev = executable(
      'gusb-umockdev-test',
      'gusb-umockdev-test.c',
      dependencies: [ umockdev, gusb_dep ])

  test('gusb-umockdev-test',
      test_umockdev,
      env: test_env)
endif
