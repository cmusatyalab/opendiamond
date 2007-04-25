noinst_LTLIBRARIES = libtools.la

libtools_la_SOURCES = ring.c rtimer_std.c rtimer.c sig_calc.c \
	rstat.c rgraph.c rcomb.c user.c file_tools.c ports.c \
	rtimer_common.h rtimer_papi.h rtimer_std.h ports.h

pkginclude_HEADERS = ring.h lib_tools.h queue.h sig_calc.h rtimer.h \
	rcomb.h rgraph.h rstat.h


noinst_PROGRAMS = test_ring test_rtimer test_rstat test_rcomb

test_ring_SOURCES = test_ring.c
test_ring_LDADD = libtools.la

test_rtimer_SOURCES = test_rtimer.c
test_rtimer_LDADD = libtools.la

test_rstat_SOURCES = test_rstat.c
test_rstat_LDADD = libtools.la

test_rcomb_SOURCES = test_rcomb.c
test_rcomb_LDADD = libtools.la
