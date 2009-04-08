noinst_LTLIBRARIES = libodisk.la

libodisk_la_SOURCES = odisk.c obj_attr.c dataretriever.c \
    odisk_priv.h obj_attr.h lib_odisk.h \
    uri_util.c uri_util.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(LIBCURL_CFLAGS)
AM_LDFLAGS = $(LIBCURL_LIBS)
