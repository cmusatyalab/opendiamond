noinst_LTLIBRARIES = libdiamondrpc.la

RPCROOTS = rpc_client_content rpc_client_scope rpc_content_scope rpc_scope_scope rpc_admin_content rpc_admin_scope

BUILT_SOURCES = $(foreach file,$(RPCROOTS),$(file)_clnt.c $(file)_svc.c $(file)_xdr.c $(file).x $(file).h)

CLEANFILES = $(BUILT_SOURCES) rpc_preamble.x

libdiamondrpc_la_SOURCES = $(filter %.c,$(BUILT_SOURCES))

AM_CPPFLAGS += -I/usr/include/tirpc -include xdr_shim.h
libdiamondrpc_la_LIBADD = -ltirpc

pkginclude_HEADERS = socket_trans.h

rpc_%_xdr.c: rpc_%.x
	rpcgen -N -C -c -o $@ $<

rpc_%.h: rpc_%.x
	rpcgen -N -C -h -o $@ $<

rpc_%_svc.c: rpc_%.x
	rpcgen -N -C -m -o $@ $<

rpc_%_clnt.c: rpc_%.x
	rpcgen -N -C -l -o $@ $<

rpc_preamble.x: rpc_preamble.x.in
	cp $< $@

%.x: %.x.in rpc_preamble.x
	cp $< $@

