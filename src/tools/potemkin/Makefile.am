noinst_PROGRAMS = potemkin 

AM_CPPFLAGS += -I/usr/include/tirpc -I$(top_builddir)/lib/transport/socket/common

potemkin_SOURCES = potemkin.c
potemkin_LDADD = ../../lib/libopendiamond.la -ltirpc
