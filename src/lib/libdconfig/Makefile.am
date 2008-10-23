AM_LFLAGS = -olex.yy.c

noinst_LTLIBRARIES = libdconfig.la

libdconfig_la_SOURCES = group_lookup.l name_lookup.l diamond_config.l \
	dconfig_tools.c gid_map.h dconfig_priv.h name_map.h

CLEANFILES = group_lookup.c name_lookup.c diamond_config.c

pkginclude_HEADERS = lib_dconfig.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS)
