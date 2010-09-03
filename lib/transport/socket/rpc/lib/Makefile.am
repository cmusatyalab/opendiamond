LIBTOOL_VERSION = 1:1:0

AM_CPPFLAGS = -I$(top_srcdir) -D_GNU_SOURCE
CFLAGS_WARNINGS = -W -Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS_WARNINGS += -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS_OTHER = -pthread
if HAVE_VISIBILITY
CFLAGS_OTHER += -fvisibility=hidden
endif
AM_CFLAGS = $(CFLAGS_WARNINGS) $(CFLAGS_OTHER)
AM_LDFLAGS = -lpthread

# Build generated XDR sources as a separate convenience library with warnings
# disabled, since rpcgen-produced code generates warnings
noinst_LTLIBRARIES = libminirpcxdr.la
libminirpcxdr_la_SOURCES = minirpc.x
nodist_libminirpcxdr_la_SOURCES = minirpc_xdr.c minirpc_xdr.h
libminirpcxdr_la_CFLAGS = $(CFLAGS_OTHER)
CLEANFILES = minirpc_xdr.c minirpc_xdr.h
BUILT_SOURCES = minirpc_xdr.h

lib_LTLIBRARIES = libminirpc.la
libminirpc_la_SOURCES = config.c connection.c message.c event.c serialize.c
libminirpc_la_SOURCES += xdr_len.c selfpipe.c pollset.c pollset_poll.c util.c
libminirpc_la_SOURCES += internal.h pollset_impl.h
libminirpc_la_LIBADD = libminirpcxdr.la
libminirpc_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBTOOL_VERSION)

%_xdr.c: %.x
	rm -f $@
	$(RPCGEN) -c $^ | \
		sed -e 's:"$(^:%.x=%\.h)":"$(notdir $(^:%.x=%_xdr.h))":g' \
		> $@ || (rm $@ && false)

%_xdr.h: %.x
	rm -f $@
	$(RPCGEN) -h -o $@ $^

# Libtool sets the .la and .so files executable, for reasons passing
# understanding.
install-exec-hook:
	chmod -x $(DESTDIR)$(libdir)/libminirpc.so
	chmod -x $(DESTDIR)$(libdir)/libminirpc.la
