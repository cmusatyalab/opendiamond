#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2007-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

AC_INIT([OpenDiamond], [6.0.5], [diamond@cs.cmu.edu], [opendiamond], [http://diamond.cs.cmu.edu/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign 1.11.1])
AM_SILENT_RULES([yes])

AC_PREREQ([2.59])
LT_PREREQ([2.2.5])

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC_C99
AM_PATH_PYTHON([2.5])

# pylint
AC_PATH_PROG([PYLINT], [pylint])
AC_ARG_VAR([PYLINT], [path to pylint])
if test z$PYLINT = z ; then
	AC_MSG_ERROR([pylint not found])
fi

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

# not really critical, only used by datamonster test program
AC_CHECK_LIB([jpeg], [jpeg_read_scanlines], [true],
	     AC_MSG_FAILURE([cannot find jpeg library]))
AC_CHECK_HEADERS(jpeglib.h)

# some options and includes
AC_SUBST(AM_CPPFLAGS, ['-D_REENTRANT -I$(top_srcdir)/lib/libfilter -DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES'])

AC_SUBST(AM_CFLAGS, ['-Wall -Wextra -Werror-implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fvisibility=hidden -Wno-unused-parameter -ffunction-sections -fdata-sections $(GLIB2_CFLAGS)'])


# done
AC_CONFIG_FILES([
Makefile
dataretriever/Makefile
dataretriever/setup.py
scopeserver/Makefile
scopeserver/setup.py
volcano/Makefile
lib/Makefile
lib/libfilter/Makefile
lib/libscope/Makefile
lib/python/Makefile
lib/python/setup.py
tests/Makefile
opendiamond.pc
])
AC_OUTPUT
