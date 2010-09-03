#
# configure.ac - build infrastructure for miniRPC
#
# Copyright (C) 2007-2008 Carnegie Mellon University
#
# This code is distributed "AS IS" without warranty of any kind under the
# terms of the GNU Lesser General Public License version 2.1, as shown in
# the file COPYING.
#

## Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([miniRPC], [0.3.3])
AM_INIT_AUTOMAKE([foreign])
AC_COPYRIGHT([Copyright (C) 2007-2008 Carnegie Mellon University])
AC_CONFIG_SRCDIR([lib/connection.c])

# Generating a config.h prevents the introduction of several lines' worth of
# -D flags on every gcc command line
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_PATH_PROG([RPCGEN], [rpcgen])
AC_ARG_VAR([RPCGEN], [RPC compiler command])
if test z$RPCGEN = z ; then
	AC_MSG_ERROR([could not locate rpcgen])
fi

AC_PATH_PROG([PERL], [perl])
AC_ARG_VAR([PERL], [path to Perl])
if test z$PERL = z ; then
	AC_MSG_ERROR([Perl not found])
fi

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.10])
PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])
CFLAGS="$CFLAGS $GLIB_CFLAGS $GTHREAD_CFLAGS"
LIBS="$LIBS $GLIB_LIBS $GTHREAD_LIBS"

# Checks for supported compiler options.
CHECK_COMPILER_OPTION([-fvisibility=hidden], [HAVE_VISIBILITY])

AC_CONFIG_FILES([minirpc.pc
		Makefile
		minirpc/Makefile
		lib/Makefile
		test/Makefile])
AC_OUTPUT
