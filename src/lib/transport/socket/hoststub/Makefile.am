noinst_LTLIBRARIES = libhoststub.la

libhoststub_la_SOURCES = hstub_api.c hstub_cntrl.c hstub_data.c \
	hstub_main.c hstub_socket.c hstub_impl.h

pkginclude_HEADERS = lib_hstub.h

AM_CPPFLAGS += -I/usr/include/tirpc -I$(top_builddir)/lib/transport/socket/common

libhoststub_la_LIBADD = -ltirpc
