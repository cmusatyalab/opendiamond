#  The OpenDiamond Platform for Interactive Search
#  Version 4
#
#  Copyright (c) 2007 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

AC_INIT([opendiamond], [4.1.0], [diamond@cs.cmu.edu])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign 1.8.5])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AM_PROG_LEX

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

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
PKG_CHECK_MODULES(GLIB2, [glib-2.0, gthread-2.0])

# sqlite3
PKG_CHECK_MODULES(SQLITE3, [sqlite3])

# minirpc
PKG_CHECK_MODULES(MINIRPC, [minirpc > 0.2])
MINIRPCGEN="`pkg-config --variable=MINIRPCGEN minirpc`"
AC_SUBST(MINIRPCGEN)

# libcurl
PKG_CHECK_MODULES(LIBCURL, [libcurl])

# libs
AC_SEARCH_LIBS([sqrt], [m],, AC_MSG_FAILURE([cannot find math library]))
AC_SEARCH_LIBS([dlopen], [dl],, AC_MSG_FAILURE([cannot find dlopen function]))
AC_SEARCH_LIBS([pthread_create],
	[pthread],, AC_MSG_FAILURE([cannot find pthread_create function]))
AC_SEARCH_LIBS([OpenSSL_add_all_digests],
	[ssl],, AC_MSG_FAILURE([cannot find ssl function]))
AC_SEARCH_LIBS([clock_gettime],
	[rt],, AC_MSG_FAILURE([cannot find POSIX clock functions]))


# some options and includes
AC_SUBST(AM_CPPFLAGS, ['-D_REENTRANT -I$(top_srcdir)/common -I$(top_srcdir)/lib/libdconfig -I$(top_srcdir)/lib/libdctl -I$(top_srcdir)/lib/libfilter -I$(top_srcdir)/lib/libfilterexec -I$(top_srcdir)/lib/liblog -I$(top_srcdir)/lib/libodisk -I$(top_srcdir)/lib/libsearchlet -I$(top_srcdir)/lib/libtools -I$(top_srcdir)/lib/transport/socket/common -I$(top_srcdir)/lib/transport/socket/hoststub -I$(top_srcdir)/lib/transport/socket/storagestub'])

AC_SUBST(AM_CFLAGS, ['-Wall -Wextra -Werror-implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fvisibility=hidden -Wno-unused-parameter $(GLIB2_CFLAGS)'])

AC_SUBST(OPENDIAMOND_LIB_CPPFLAGS, ['-DG_LOG_DOMAIN=\"OpenDiamond\"'])


# done
AC_CONFIG_FILES([
Makefile
adiskd/Makefile
common/Makefile
sqlite/Makefile
lib/Makefile
lib/libdconfig/Makefile
lib/libdctl/Makefile
lib/libfilter/Makefile
lib/libfilterexec/Makefile
lib/liblog/Makefile
lib/libodisk/Makefile
lib/libscope/Makefile
lib/libsearchlet/Makefile
lib/libtools/Makefile
lib/transport/Makefile
lib/transport/socket/Makefile
lib/transport/socket/common/Makefile
lib/transport/socket/hoststub/Makefile
lib/transport/socket/storagestub/Makefile
tools/Makefile
tools/dctl/Makefile
tools/volcano/Makefile
tests/Makefile
debian/Makefile
target/Makefile
target/bin/Makefile
opendiamond.pc
])
AC_OUTPUT
