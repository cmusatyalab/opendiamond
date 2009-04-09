noinst_LTLIBRARIES = libstoragestub.la

libstoragestub_la_SOURCES = sstub_api.c sstub_cntrl.c sstub_conn.c \
	sstub_data.c sstub_listen.c sstub_impl.h lib_sstub.h

AM_CPPFLAGS += -I$(top_builddir)/lib/transport/socket/common

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(LIBSOUP_CFLAGS)
