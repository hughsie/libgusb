project('libgusb', 'c',
  version : '0.4.9',
  license : 'LGPL-2.1-or-later',
  meson_version : '>=0.56.0',
  default_options : ['c_std=c99']
)

gusb_version = meson.project_version()
varr = gusb_version.split('.')
gusb_major_version = varr[0]
gusb_minor_version = varr[1]
gusb_micro_version = varr[2]

conf = configuration_data()
conf.set_quoted('VERSION', gusb_version)

# libtool versioning - this applies to libgusb
lt_current = 2
lt_revision = 10
lt_age = 0
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

# get supported warning flags
test_args = [
  '-Waggregate-return',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wempty-body',
  '-Wextra',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wno-gnu-variable-sized-type-not-at-end',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpointer-arith',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
foreach arg: test_args
  if cc.has_argument(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

if not meson.is_cross_build()
  add_project_arguments('-fstack-protector-strong', language : 'c')
endif

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach arg: test_link_args
  if cc.has_link_argument(arg)
    global_link_args += arg
  endif
endforeach
add_project_link_arguments(
  global_link_args,
  language: 'c'
)

if get_option('tests')
  umockdev = dependency('umockdev-1.0', version: '>= 0.17.7', disabler: true, required: get_option('umockdev'))
else
  umockdev = disabler()
endif
libgio = dependency('gio-2.0', version : '>= 2.44.0')
libusb = dependency('libusb-1.0', version : '>= 1.0.9')
if cc.has_header_symbol('libusb.h', 'LIBUSB_CAP_HAS_HOTPLUG', dependencies: libusb)
  conf.set('HAVE_LIBUSB_CAP_HAS_HOTPLUG', '1')
endif
if cc.has_header_symbol('libusb.h', 'libusb_has_capability', dependencies: libusb)
  conf.set('HAVE_LIBUSB_HAS_CAPABILITY', '1')
endif
if cc.has_header_symbol('libusb.h', 'libusb_set_option', dependencies: libusb)
  conf.set('HAVE_LIBUSB_SET_OPTION', '1')
endif
if cc.has_header_symbol('libusb.h', 'libusb_get_parent', dependencies: libusb)
  conf.set('HAVE_LIBUSB_GET_PARENT', '1')
endif
if cc.has_header_symbol('libusb.h', 'libusb_get_port_number', dependencies: libusb)
  conf.set('HAVE_LIBUSB_GET_PORT_NUMBER', '1')
endif
libjsonglib = dependency('json-glib-1.0', version: '>= 1.1.1')

gusb_deps = [
  libgio,
  libusb,
  libjsonglib,
]

if not get_option('introspection') and get_option('vapi')
  error('-Dvapi=true requires -Dintrospection=true')
endif

gnome = import('gnome')

add_project_arguments('-DGUSB_COMPILATION', language: 'c')

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))

configure_file(
  output : 'config.h',
  configuration : conf
)

root_incdir = include_directories('.')

subdir('gusb')
subdir('tools')
if get_option('docs')
  subdir('docs')
endif
