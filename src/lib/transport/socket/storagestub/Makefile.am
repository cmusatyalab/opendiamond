noinst_LTLIBRARIES = libstoragestub.la

libstoragestub_la_SOURCES = sstub_api.c sstub_cntrl.c sstub_conn.c \
	sstub_data.c sstub_listen.c sstub_impl.h sstub_tirpc.c \
	../common/rpc_interface.h

AM_CPPFLAGS += -I/usr/include/tirpc -I$(top_builddir)/lib/transport/socket/common

libstoragestub_la_LIBADD = -ltirpc

pkginclude_HEADERS = lib_sstub.h
