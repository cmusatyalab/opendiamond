#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2007-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

AC_INIT([OpenDiamond], [10.0.0], [diamond@cs.cmu.edu], [opendiamond], [http://diamond.cs.cmu.edu/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign 1.11.1 subdir-objects])
AM_SILENT_RULES([yes])

AC_PREREQ([2.59])
LT_PREREQ([2.2.5])

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC_C99


# tox
AC_PATH_PROG([TOX], [tox], [:])
AC_ARG_VAR([TOX], [path to tox])

# xml2rfc
AC_PATH_PROG([XML2RFC], [xml2rfc], [false])
AC_ARG_VAR([XML2RFC], [path to xml2rfc])

LT_INIT()

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

# libs
AC_SEARCH_LIBS([pthread_create],
	[pthread],, AC_MSG_FAILURE([cannot find pthread_create function]))

# some options and includes
# The min/max glib version is actually 2.12, but glib doesn't have special
# handling for API changes that old
AC_SUBST(AM_CPPFLAGS, ['-D_REENTRANT -I$(top_srcdir)/lib/libfilter -DG_DISABLE_SINGLE_INCLUDES -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26 -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_MIN_REQUIRED'])

AC_SUBST(AM_CFLAGS, ['-Wall -Wextra -Werror-implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fvisibility=hidden -Wno-unused-parameter -ffunction-sections -fdata-sections $(GLIB2_CFLAGS)'])


# done
AC_CONFIG_FILES([
Makefile
libfilter/Makefile
newscope/Makefile
tests/Makefile
opendiamond.pc
opendiamond/__init__.py
])
AC_OUTPUT
