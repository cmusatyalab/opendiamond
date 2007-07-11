noinst_LTLIBRARIES = liblog.la

liblog_la_SOURCES = lib_log.c

pkginclude_HEADERS = lib_log.h

noinst_PROGRAMS = test_log

test_log_SOURCES = test_log.c
test_log_LDADD = liblog.la ../libdconfig/libdconfig.la \
	../libdctl/libdctl.la ${GLIB2_LIBS}
