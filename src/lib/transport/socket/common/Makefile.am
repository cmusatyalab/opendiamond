noinst_LTLIBRARIES = libdiamondrpc.la

RPCROOTS = rpc_client_content rpc_client_scope rpc_content_scope rpc_scope_scope rpc_admin_content rpc_admin_scope

BUILT_SOURCES = $(foreach file,$(RPCROOTS),$(file)_clnt.c $(file)_svc.c $(file)_xdr.c $(file).x $(file).h)

CLEANFILES = $(BUILT_SOURCES) rpc_preamble.x rpc_preamble_xdr.c

libdiamondrpc_la_SOURCES = rpc_client_content_svc.c rpc_client_scope_svc.c \
		 rpc_content_scope_svc.c rpc_scope_scope_svc.c \
		 rpc_admin_content_svc.c rpc_admin_scope_svc.c \
		 rpc_client_content_clnt.c rpc_client_scope_clnt.c \
		 rpc_content_scope_clnt.c rpc_scope_scope_clnt.c \
		 rpc_admin_content_clnt.c rpc_admin_scope_clnt.c \
		 rpc_client_content_xdr.c rpc_client_scope_xdr.c \
		 rpc_content_scope_xdr.c rpc_scope_scope_xdr.c \
		 rpc_admin_content_xdr.c rpc_admin_scope_xdr.c \
		 rpc_preamble_xdr.c

AM_CPPFLAGS += -I/usr/include/tirpc -include xdr_shim.h
libdiamondrpc_la_LIBADD = -ltirpc

pkginclude_HEADERS = socket_trans.h

# This string is used to store a perl command which strips out duplicate
# XDR functions from generated files.
ugly_perl_command = "undef $$/; while(<>) { s/bool_t\nxdr_begin_preamble.*bool_t\nxdr_end_preamble[^}]*}//s; print; }"

rpc_preamble.h: rpc_preamble.x
	$(RM) $@
	rpcgen -N -C -h -o $@ $<

rpc_preamble_xdr.c: rpc_preamble.x rpc_preamble.h
	$(RM) $@
	rpcgen -N -C -c -o $@ $<

rpc_%_xdr.c: rpc_%.x
	$(RM) $@
	rpcgen -N -C -c -o $@ $<
	perl -i -e $(ugly_perl_command) $@

rpc_%.h: rpc_%.x
	$(RM) $@
	rpcgen -N -C -h -o $@ $<

rpc_%_svc.c: rpc_%.x
	$(RM) $@
	rpcgen -N -C -m -o $@ $<

rpc_%_clnt.c: rpc_%.x
	$(RM) $@
	rpcgen -N -C -l -o $@ $<

rpc_preamble.x: rpc_preamble.x.in
	cp $< $@

%.x: %.x.in rpc_preamble.x
	cp $< $@

