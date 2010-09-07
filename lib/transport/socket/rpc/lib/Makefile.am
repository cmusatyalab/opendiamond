AM_CPPFLAGS += -D_GNU_SOURCE
AM_CFLAGS += -Wno-missing-field-initializers
AM_LDFLAGS = $(GLIB2_LIBS) -lpthread

# Build generated XDR sources as a separate convenience library with warnings
# disabled, since rpcgen-produced code generates warnings
noinst_LTLIBRARIES = libminirpcxdr.la
libminirpcxdr_la_SOURCES = minirpc.x
nodist_libminirpcxdr_la_SOURCES = minirpc_xdr.c minirpc_xdr.h
libminirpcxdr_la_CFLAGS = -pthread -fvisibility=hidden
CLEANFILES = minirpc_xdr.c minirpc_xdr.h
BUILT_SOURCES = minirpc_xdr.h

noinst_LTLIBRARIES += libminirpc.la
libminirpc_la_SOURCES = config.c connection.c message.c event.c serialize.c
libminirpc_la_SOURCES += xdr_len.c selfpipe.c pollset.c pollset_poll.c util.c
libminirpc_la_SOURCES += internal.h pollset_impl.h
libminirpc_la_LIBADD = libminirpcxdr.la

%_xdr.c: %.x
	rm -f $@
	$(RPCGEN) -c $^ | \
		sed -e 's:"$(^:%.x=%\.h)":"$(notdir $(^:%.x=%_xdr.h))":g' \
		> $@ || (rm $@ && false)

%_xdr.h: %.x
	rm -f $@
	$(RPCGEN) -h -o $@ $^
