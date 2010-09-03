AM_CPPFLAGS = -I$(top_srcdir)
CFLAGS_WARNINGS = -W -Wall -Wstrict-prototypes -Wno-unused-parameter
CFLAGS_WARNINGS += -Wno-missing-field-initializers
CFLAGS_OTHER = -pthread
AM_CFLAGS = $(CFLAGS_WARNINGS) $(CFLAGS_OTHER)
AM_LDFLAGS = -L$(top_builddir)/lib -lminirpc -lpthread

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
libxdr_a_CFLAGS = $(CFLAGS_OTHER)
libcommon_a_SOURCES  = common.c common.h client_sync.c client_async.c
libcommon_a_SOURCES += server_sync.c server_async.c proto.mx
nodist_libcommon_a_SOURCES = $(GEN_NOXDR)
LDADD = libcommon.a libxdr.a

check_SCRIPTS = runtest.sh
CLEANFILES += $(check_SCRIPTS)
EXTRA_DIST = $(check_SCRIPTS:=.in) valgrind.supp
$(check_SCRIPTS): %: %.in Makefile
	@echo "Generating $@ from $<"
	@sed -e "s=!!ENABLE_VALGRIND!!=$(enable_valgrind)=g" \
				-e "s=!!VALGRIND!!=$(VALGRIND)=g" \
				-e "s=!!LIBTOOL!!=$(LIBTOOL)=g" \
				-e "s=!!SRCDIR!!=$(srcdir)=g" $< > $@
	@chmod +x $@

check_PROGRAMS  = test_basic
check_PROGRAMS += test_async_server
check_PROGRAMS += test_async_client
check_PROGRAMS += test_async_both
check_PROGRAMS += test_sequential_clients
check_PROGRAMS += test_simultaneous_clients
check_PROGRAMS += test_set_ops
check_PROGRAMS += test_bind_fd
check_PROGRAMS += test_unix
check_PROGRAMS += test_plug
check_PROGRAMS += test_dispatch
check_PROGRAMS += test_refcount
check_PROGRAMS += test_shutdown
check_PROGRAMS += test_accept_close
check_PROGRAMS += test_simultaneous_close
check_PROGRAMS += test_exec_close
check_PROGRAMS += test_errors
check_PROGRAMS += test_max_buf
check_PROGRAMS += test_protocol_err
check_PROGRAMS += test_fd_limit
check_PROGRAMS += test_throughput

TESTS_ENVIRONMENT = $(builddir)/runtest.sh
TESTS = $(check_PROGRAMS)

%_minirpc.c %_minirpc.h %_client.h %_server.h %_xdr.c %_xdr.h: %.mx $(top_builddir)/minirpcgen
	$(top_builddir)/minirpcgen $(filter-out %minirpcgen,$^)
