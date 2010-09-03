AM_CPPFLAGS = -I$(top_srcdir) -D_GNU_SOURCE
AM_CFLAGS   = -W -Wall -Wstrict-prototypes -Wno-unused-parameter
AM_CFLAGS  += -Wno-missing-field-initializers -pthread
AM_LDFLAGS  = -L$(top_builddir)/lib -lminirpc -lpthread

GEN_C = example_minirpc.c example_xdr.c
GEN_H = example_minirpc.h example_xdr.h example_client.h example_server.h
GEN = $(GEN_C) $(GEN_H)
CLEANFILES = $(GEN)
BUILT_SOURCES = $(GEN_H)

EXTRA_PROGRAMS = example_client example_server
example_server_SOURCES = example_server.c example.mx
nodist_example_server_SOURCES = $(GEN)
example_client_SOURCES = example_client.c example.mx
nodist_example_client_SOURCES = $(GEN)

CLEANFILES += $(EXTRA_PROGRAMS)

%_minirpc.c %_minirpc.h %_client.h %_server.h %_xdr.c %_xdr.h: %.mx $(top_builddir)/minirpcgen
	$(top_builddir)/minirpcgen $(filter-out %minirpcgen,$^)
