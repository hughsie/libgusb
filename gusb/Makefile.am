
AM_CPPFLAGS = \
	$(GLIB_CFLAGS)						\
	$(USB_CFLAGS)						\
	$(GUDEV_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/gusb					\
	-DGUSB_COMPILATION					\
	-DG_LOG_DOMAIN=\"libgusb\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

lib_LTLIBRARIES =						\
	libgusb.la

libgusb_includedir = $(includedir)/gusb-1
libgusb_include_HEADERS =					\
	gusb-private.h						\
	gusb.h

libgusbbase_includedir = $(libgusb_includedir)/gusb
libgusbbase_include_HEADERS =					\
	gusb-version.h						\
	gusb-context.h						\
	gusb-context-private.h					\
	gusb-device.h						\
	gusb-device-private.h					\
	gusb-device-list.h					\
	gusb-source.h						\
	gusb-source-private.h					\
	gusb-util.h

libgusb_la_SOURCES =						\
	gusb-context.c						\
	gusb-device.c						\
	gusb-device-list.c					\
	gusb-source.c						\
	gusb-util.c

libgusb_la_LIBADD =						\
	$(GLIB_LIBS)						\
	$(USB_LIBS)						\
	$(GUDEV_LIBS)

libgusb_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^g_usb_.*'

libgusb_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gusb.pc

if GUSB_BUILD_TESTS
check_PROGRAMS =						\
	gusb-self-test

gusb_self_test_SOURCES =					\
	gusb-self-test.c

gusb_self_test_LDADD =						\
	$(GLIB_LIBS)						\
	$(USB_LIBS)						\
	$(lib_LTLIBRARIES)

gusb_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gusb-self-test
endif

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS = GUsb-1.0.gir

GUsb-1.0.gir: libgusb.la

GUsb_1_0_gir_PACKAGES = glib-2.0 gobject-2.0 libusb-1.0
if HAVE_GUDEV
GUsb_1_0_gir_PACKAGES += gudev-1.0
endif
GUsb_1_0_gir_EXPORT_PACKAGES = gusb
GUsb_1_0_gir_INCLUDES = GLib-2.0 GObject-2.0 Gio-2.0
GUsb_1_0_gir_LIBS = libgusb.la
GUsb_1_0_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -DG_USB_API_IS_SUBJECT_TO_CHANGE -DGUSB_COMPILATION
GUsb_1_0_gir_FILES = $(libgusb_la_SOURCES) $(libgusbbase_include_HEADERS)
GUsb_1_0_gir_SCANNERFLAGS = --c-include "gusb.h"

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)
endif

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

gusb.vapi: GUsb-1.0.gir

VAPIGEN_VAPIS = gusb.vapi

gusb_vapi_DEPS = gio-2.0
gusb_vapi_METADATADIRS = $(srcdir)
gusb_vapi_FILES = GUsb-1.0.gir

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS)
endif

EXTRA_DIST =							\
	gusb-version.h.in					\
	gusb.pc.in

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
CLEANFILES += $(INTROSPECTION_GIRS) $(INTROSPECTION_GIRS:.gir=.typelib)
endif
if ENABLE_VAPIGEN
CLEANFILES += $(VAPIGEN_VAPIS)
endif

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
