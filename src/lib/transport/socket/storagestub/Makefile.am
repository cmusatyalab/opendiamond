noinst_LTLIBRARIES = libstoragestub.la

libstoragestub_la_SOURCES = sstub_api.c sstub_cntrl.c sstub_conn.c \
	sstub_data.c sstub_listen.c sstub_impl.h sstub_rpc.c

AM_CPPFLAGS += -I$(top_builddir)/lib/transport/socket/common

pkginclude_HEADERS = lib_sstub.h
