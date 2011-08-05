# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

m4_define([libgusb_major_version], [0])
m4_define([libgusb_minor_version], [0])
m4_define([libgusb_micro_version], [1])
m4_define([libgusb_version],
          [libgusb_major_version.libgusb_minor_version.libgusb_micro_version])

AC_INIT([libgusb],[libgusb_version],[http://www.hughsie.com])
AC_CONFIG_SRCDIR(libgusb)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_PROG_LIBTOOL

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar])

# set up gtk-doc
GTK_DOC_CHECK(1.9)

# use this in cd-version.h
LIBGUSB_MAJOR_VERSION=libgusb_major_version
LIBGUSB_MINOR_VERSION=libgusb_minor_version
LIBGUSB_MICRO_VERSION=libgusb_micro_version
AC_SUBST(VERSION)
AC_SUBST(LIBGUSB_MAJOR_VERSION)
AC_SUBST(LIBGUSB_MINOR_VERSION)
AC_SUBST(LIBGUSB_MICRO_VERSION)

# libtool versioning - this applies to libgusb
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		Don't use.
LT_CURRENT=1
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_INSTALL
LT_INIT
AC_PATH_PROG(XSLTPROC, xsltproc)

dnl ---------------------------------------------------------------------------
dnl - Use strict options (default enabled for devs, disabled in releases)
dnl ---------------------------------------------------------------------------
if test -d ".git"; then
	default_strict=yes
else
	default_strict=no
fi

AC_ARG_ENABLE(strict, AS_HELP_STRING([--enable-strict],
				     [Enable strict compilation options]),
				     enable_strict=$enableval,
				     enable_strict=$default_strict)
if test x$enable_strict != xno; then
	if test "$GCC" = "yes"; then
		CPPFLAGS="$CPPFLAGS -Werror"
	fi
fi

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.28.0 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9)

dnl **** Check for GUDEV ****
AC_ARG_ENABLE(gudev, AS_HELP_STRING([--enable-gudev],[Enable GUDEV support]),
	      enable_gudev=$enableval, enable_gudev=yes)
if test x$enable_gudev != xno; then
	PKG_CHECK_MODULES(GUDEV, gudev-1.0)
	AC_DEFINE(HAVE_GUDEV,1,[Use GUDEV support for hardware])
fi
AM_CONDITIONAL(HAVE_GUDEV, test x$enable_gudev = xyes)

PKG_CHECK_MODULES(USB, libusb-1.0 >= 1.0.9, has_new_libusb=yes, has_new_libusb=no)
if test x$has_new_libusb = xyes; then
	AC_DEFINE(HAVE_NEW_USB,1,[Got new libusb])
else
	PKG_CHECK_MODULES(USB, libusb-1.0 >= 1.0.0)
fi

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
libgusb/gusb-version.h
libgusb/gusb.pc
libgusb/Makefile
])
AC_OUTPUT

dnl ==========================================================================
echo "
                    libgusb $VERSION
                  ====================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
        GUDEV support:             ${enable_gudev}
"
