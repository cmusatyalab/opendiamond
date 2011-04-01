lib_LTLIBRARIES = libdiamondfilter.la

libdiamondfilter_la_SOURCES  = lib_filter.c lf_protocol.c lf_wrapper.c
libdiamondfilter_la_SOURCES += lf_priv.h lf_protocol.h

libdiamondfilter_la_LDFLAGS = -version-info 0:0:0

libdiamondfilter_la_LIBADD = ${GLIB2_LIBS}

pkginclude_HEADERS = lib_filter.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS)
