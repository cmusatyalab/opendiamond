AM_CFLAGS += -Wno-missing-field-initializers
AM_LDFLAGS = $(GLIB2_LIBS) -L$(builddir)/../lib -lminirpc -lpthread

# Build generated XDR sources as a separate convenience library with warnings
# disabled, since rpcgen-produced code generates warnings
GEN_NOXDR_C = proto_minirpc.c
GEN_NOXDR_H = proto_minirpc.h proto_client.h proto_server.h
GEN_NOXDR = $(GEN_NOXDR_C) $(GEN_NOXDR_H)
GEN_XDR_C = proto_xdr.c
GEN_XDR_H = proto_xdr.h
GEN_XDR = $(GEN_XDR_C) $(GEN_XDR_H)
GEN = $(GEN_NOXDR) $(GEN_XDR)
CLEANFILES = $(GEN)
BUILT_SOURCES = $(GEN_NOXDR_H) $(GEN_XDR_H)

check_LIBRARIES = libcommon.a libxdr.a
libxdr_a_SOURCES = proto.mx
nodist_libxdr_a_SOURCES = $(GEN_XDR)
libxdr_a_CFLAGS = -pthread
libcommon_a_SOURCES  = common.c common.h client_sync.c server_sync.c proto.mx
nodist_libcommon_a_SOURCES = $(GEN_NOXDR)
LDADD = libcommon.a libxdr.a

check_PROGRAMS  = test_basic
check_PROGRAMS += test_sequential_clients
check_PROGRAMS += test_set_ops
check_PROGRAMS += test_bind_fd
check_PROGRAMS += test_unix
check_PROGRAMS += test_shutdown
check_PROGRAMS += test_simultaneous_close
check_PROGRAMS += test_exec_close
check_PROGRAMS += test_errors
# check_PROGRAMS += test_protocol_err

TESTS_ENVIRONMENT = $(srcdir)/runtest.sh
TESTS = $(check_PROGRAMS)
EXTRA_DIST = $(TESTS_ENVIRONMENT)

%_minirpc.c %_minirpc.h %_client.h %_server.h %_xdr.c %_xdr.h: %.mx $(builddir)/../minirpcgen
	$(builddir)/../minirpcgen $(filter-out %minirpcgen,$^)
