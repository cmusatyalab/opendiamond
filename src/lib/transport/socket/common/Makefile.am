noinst_LTLIBRARIES = libdiamondrpc.la

MINIRPC_FILES = rpc_client_content.mx rpc_preamble.mx
libdiamondrpc_la_SOURCES = socket_trans.c socket_trans.h $(MINIRPC_FILES)

MINIRPC_GENERATED = \
	${MINIRPC_FILES:.mx=_client.h} ${MINIRPC_FILES:.mx=_server.h} \
	${MINIRPC_FILES:.mx=_minirpc.c} ${MINIRPC_FILES:.mx=_minirpc.h} \
	${MINIRPC_FILES:.mx=_xdr.c} ${MINIRPC_FILES:.mx=_xdr.h}

nodist_libdiamondrpc_la_SOURCES = ${MINIRPC_GENERATED}

BUILT_SOURCES = ${MINIRPC_GENERATED}
CLEANFILES = ${MINIRPC_GENERATED}

# make sure we rebuild rpc interfaces when the common header changes
rpc_client_content_xdr.h: rpc_preamble_xdr.h

%_client.h %_server.h %_minirpc.c %_minirpc.h %_xdr.c %_xdr.h: %.mx
	$(MINIRPCGEN) $<

