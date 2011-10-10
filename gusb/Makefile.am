
INCLUDES = \
	$(GLIB_CFLAGS)						\
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

EXTRA_DIST =							\
	gusb-version.h.in					\
	gusb.pc.in

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
