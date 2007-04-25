AM_LFLAGS = -olex.yy.c

noinst_LTLIBRARIES = libdconfig.la

libdconfig_la_SOURCES = group_lookup.l name_lookup.l diamond_config.l \
	dconfig_tools.c gid_map.h dconfig_priv.h name_map.h

pkginclude_HEADERS = lib_dconfig.h


noinst_PROGRAMS = name_test dconf_test

name_test_SOURCES = name_test.c
name_test_LDADD = libdconfig.la

dconf_test_SOURCES = dconf_test.c
dconf_test_LDADD = libdconfig.la
