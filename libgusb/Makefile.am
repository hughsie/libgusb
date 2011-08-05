
INCLUDES = \
	$(GLIB_CFLAGS)						\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libgusb					\
	-DGUSB_COMPILATION					\
	-DG_LOG_DOMAIN=\"libgusb\"				\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

lib_LTLIBRARIES =						\
	libgusb.la

libgusb_includedir = $(includedir)/gusb-1
libgusb_include_HEADERS =					\
	gusb.h

libgusbbase_includedir = $(libgusb_includedir)/gusb
libgusbbase_include_HEADERS =					\
	gusb-version.h						\
	gusb-device.h						\
	gusb-source.h

libgusb_la_SOURCES =						\
	gusb-device.c						\
	gusb-source.c

libgusb_la_LIBADD =						\
	$(GLIB_LIBS)

libgusb_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^g_usb_.*'

libgusb_la_CFLAGS =						\
	$(WARNINGFLAGS_C)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gusb.pc

EXTRA_DIST =							\
	gusb-version.h.in					\
	gusb.pc.in

CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -f *~

-include $(top_srcdir)/git.mk
