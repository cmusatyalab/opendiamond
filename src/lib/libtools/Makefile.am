noinst_LTLIBRARIES = libtools.la

libtools_la_SOURCES = ring.c rtimer.c sig_calc.c \
	rgraph.c rcomb.c user.c file_tools.c ports.c ports.h \
	rtimer.h sig_calc_priv.h tools_priv.h rcomb.h rgraph.h ring.h \
	g_checksum_compat.c g_checksum_compat.h

pkginclude_HEADERS = lib_tools.h sig_calc.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS)
