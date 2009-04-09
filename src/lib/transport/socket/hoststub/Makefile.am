noinst_LTLIBRARIES = libhoststub.la

libhoststub_la_SOURCES = hstub_api.c hstub_data.c hstub_main.c hstub_socket.c \
	hstub_impl.h lib_hstub.h

AM_CPPFLAGS += -I$(top_builddir)/lib/transport/socket/common

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(LIBSOUP_CFLAGS)
