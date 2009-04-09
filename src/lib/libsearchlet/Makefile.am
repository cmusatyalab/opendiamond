noinst_LTLIBRARIES = libsearchlet.la

libsearchlet_la_SOURCES = lib_searchlet.c ls_background.c ls_device.c \
	ls_dctl.c log_impl.h lib_search_priv.h dctl_socket.h log_socket.h

pkginclude_HEADERS = lib_searchlet.h

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(LIBSOUP_CFLAGS)
