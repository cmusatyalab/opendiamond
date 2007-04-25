noinst_LTLIBRARIES = libsearchlet.la

libsearchlet_la_SOURCES = lib_searchlet.c ls_background.c ls_device.c \
	ls_log.c ls_dctl.c log_impl.h lib_search_priv.h

pkginclude_HEADERS = lib_searchlet.h dctl_socket.h log_socket.h


noinst_PROGRAMS = cli_test

cli_test_SOURCES = cli_test.c test.c
cli_test_LDADD = libsearchlet.la ../libfilter/libfilter.la \
	../liblog/liblog.la ../libdctl/libdctl.la \
	../transport/libtransport.la \
	../libauth/libauth.la ../libdconfig/libdconfig.la \
	../libfilterexec/libfilterexec.la ../libodisk/libodisk.la \
	../libtools/libtools.la


# NOTE: we no longer build test.so as in the old Makefile.in
