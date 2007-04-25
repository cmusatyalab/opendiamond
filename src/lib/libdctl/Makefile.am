noinst_LTLIBRARIES = libdctl.la

libdctl_la_SOURCES = lib_dctl.c dctl_helper.c dctl_impl.h

pkginclude_HEADERS = lib_dctl.h dctl_common.h


noinst_PROGRAMS = test_dctl

test_dctl_SOURCES = test_dctl.c
test_dctl_LDADD = libdctl.la
