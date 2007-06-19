noinst_LTLIBRARIES = libdiamondrpc.la

BUILT_SOURCES = rpc_interface_clnt.c rpc_interface_xdr.c \
	rpc_interface.h rpc_interface_svc.c

libdiamondrpc_la_SOURCES = rpc_interface_xdr.c rpc_interface_svc.c \
	rpc_interface_clnt.c

AM_CPPFLAGS += -I/usr/include/tirpc -include xdr_shim.h
libdiamondrpc_la_LIBADD = -ltirpc

pkginclude_HEADERS = socket_trans.h

CLEANFILES = rpc_interface_clnt.c rpc_interface_xdr.c rpc_interface.h \
	rpc_interface_svc.c rpc_interface.x

rpc_interface_xdr.c: rpc_interface.x
	rpcgen -N -C -c -o rpc_interface_xdr.c rpc_interface.x

rpc_interface.h: rpc_interface.x
	rpcgen -N -C -h -o rpc_interface.h rpc_interface.x

rpc_interface_svc.c: rpc_interface.x
	rpcgen -N -C -m -o rpc_interface_svc.c rpc_interface.x

rpc_interface_clnt.c: rpc_interface.x
	rpcgen -N -C -l -o rpc_interface_clnt.c rpc_interface.x

rpc_interface.x: rpc_interface.x.in
	cp $< $@