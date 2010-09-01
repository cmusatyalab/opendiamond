#  The OpenDiamond Platform for Interactive Search
#  Version 5
#
#  Copyright (c) 2007-2010 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

AC_INIT([OpenDiamond], [5.2.1], [diamond@cs.cmu.edu], [opendiamond], [http://diamond.cs.cmu.edu/])
AM_INIT_AUTOMAKE([foreign 1.11.1 dist-xz])
AM_SILENT_RULES([yes])

AC_PREREQ([2.59])
LT_PREREQ([2.2.5])

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC_C99
AM_PROG_LEX
AM_PATH_PYTHON([2.5])

LT_INIT([disable-static])

# Check for recent pkg-config which supports Requires.private
# (snippet taken from cairo configure.in)
PKG_PROG_PKG_CONFIG()
if test "x$PKG_CONFIG" = x; then
        AC_MSG_ERROR([pkg-config >= $PKGCONFIG_REQUIRED required but not found (http://pkgconfig.freedesktop.org/)])
fi

case `$PKG_CONFIG --version` in
[0.?|0.?.?|0.1[0-7]|0.1[0-7].?]) PKGCONFIG_REQUIRES="Requires"; ;;
*) PKGCONFIG_REQUIRES="Requires.private"; ;;
esac

AC_SUBST(PKGCONFIG_REQUIRES)

# glib
PKG_CHECK_MODULES(GLIB2, [glib-2.0 >= 2.12, gthread-2.0])
PKG_CHECK_EXISTS([glib-2.0 < 2.16],
    [ AC_DEFINE([HAVE_GLIB2_OLD], [1], [Have a old version of glib-2.0]) ])

# minirpc
PKG_CHECK_MODULES(MINIRPC, [minirpc > 0.2])
MINIRPCGEN="`pkg-config --variable=MINIRPCGEN minirpc`"
AC_SUBST(MINIRPCGEN)

# libsoup
PKG_CHECK_EXISTS([libsoup-2.4],
    [ PKG_CHECK_MODULES(LIBSOUP, [libsoup-2.4 >= 2.4.1]) ],
    [ PKG_CHECK_MODULES(LIBSOUP, [libsoup-2.2 >= 2.2.100],
	[ AC_DEFINE([HAVE_LIBSOUP22], [1], [Have libsoup version 2.2]) ]) ])

# scopecookie libaries, gnutls + uuid
PKG_CHECK_MODULES(GNUTLS, [gnutls])
PKG_CHECK_MODULES(UUID, [uuid])

# libs
AC_SEARCH_LIBS([sqrt], [m],, AC_MSG_FAILURE([cannot find math library]))
AC_SEARCH_LIBS([dlopen], [dl],, AC_MSG_FAILURE([cannot find dlopen function]))
AC_SEARCH_LIBS([pthread_create],
	[pthread],, AC_MSG_FAILURE([cannot find pthread_create function]))
AC_SEARCH_LIBS([clock_gettime],
	[rt],, AC_MSG_FAILURE([cannot find POSIX clock functions]))

# not really critical, only used by datamonster test program
AC_CHECK_LIB([jpeg], [jpeg_read_scanlines], [true],
	     AC_MSG_FAILURE([cannot find jpeg library]))
AC_CHECK_HEADERS(jpeglib.h)

# some options and includes
AC_SUBST(AM_CPPFLAGS, ['-D_REENTRANT -I$(top_srcdir)/common -I$(top_srcdir)/lib/libdconfig -I$(top_srcdir)/lib/libdctl -I$(top_srcdir)/lib/libfilter -I$(top_srcdir)/lib/libfilterexec -I$(top_srcdir)/lib/liblog -I$(top_srcdir)/lib/libodisk -I$(top_srcdir)/lib/libscope -I$(top_srcdir)/lib/libtools -I$(top_srcdir)/lib/transport/socket/common -I$(top_srcdir)/lib/transport/socket/storagestub -DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES'])

AC_SUBST(AM_CFLAGS, ['-Wall -Wextra -Werror-implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fvisibility=hidden -Wno-unused-parameter $(GLIB2_CFLAGS)'])

AC_SUBST(OPENDIAMOND_LIB_CPPFLAGS, ['-DG_LOG_DOMAIN=\"OpenDiamond\"'])


# done
AC_CONFIG_FILES([
Makefile
adiskd/Makefile
common/Makefile
dataretriever/Makefile
dataretriever/setup.py
scopeserver/Makefile
scopeserver/setup.py
sqlite/Makefile
filter-runner/Makefile
volcano/Makefile
lib/Makefile
lib/libdconfig/Makefile
lib/libfilter/Makefile
lib/libfilterexec/Makefile
lib/liblog/Makefile
lib/libodisk/Makefile
lib/libscope/Makefile
lib/libtools/Makefile
lib/transport/Makefile
lib/transport/socket/Makefile
lib/transport/socket/common/Makefile
lib/transport/socket/storagestub/Makefile
tests/Makefile
target/Makefile
target/bin/Makefile
opendiamond.pc
])
AC_OUTPUT
