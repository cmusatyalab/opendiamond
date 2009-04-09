noinst_LTLIBRARIES = libodisk.la

libodisk_la_SOURCES = odisk.c obj_attr.c dataretriever.c \
    odisk_priv.h obj_attr.h lib_odisk.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(LIBSOUP_CFLAGS)
AM_LDFLAGS = $(LIBSOUP_LIBS)
