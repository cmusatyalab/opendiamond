noinst_LTLIBRARIES = libod.la

libod_la_SOURCES = od_api.c od_lookup.c od_clnt.c od_xdr.c od_priv.h \
	 od.x.in lib_od.h.in


noinst_PROGRAMS = od_server otest apitest

BUILT_SOURCES = od_clnt.c od_xdr.c od.h od_svc.c

od_server_SOURCES = od_svc.c od_server.c
od_server_LDADD = libod.la ../liblog/liblog.la ../libdctl/libdctl.la \
	../libodisk/libodisk.la ../libdconfig/libdconfig.la \
	../libtools/libtools.la ../libdctl/libdctl.la

otest_SOURCES = otest.c
otest_LDADD = libod.la ../libdconfig/libdconfig.la

apitest_SOURCES = apitest.c
apitest_LDADD = libod.la ../libdconfig/libdconfig.la


# rpcgen stuff (includes hack for rpcgen's interesting behavior with paths)
CLEANFILES = od_clnt.c od_xdr.c od.h od_svc.c od.x lib_od.h
od_clnt.c od_xdr.c od.h od_svc.c: od.x lib_od.h
	rpcgen od.x

od.x: od.x.in
	cp $< $@

lib_od.h: lib_od.h.in
	cp $< $@
