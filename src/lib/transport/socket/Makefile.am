SUBDIRS = common storagestub hoststub

noinst_LTLIBRARIES = libsocket.la

libsocket_la_SOURCES =
libsocket_la_LIBADD = common/libdiamondrpc.la storagestub/libstoragestub.la \
	hoststub/libhoststub.la ${MINIRPC_LIBS}
