bin_PROGRAMS = volcano

volcano_LDADD = $(top_builddir)/lib/libopendiamond.la  -lsqlite3
AM_CPPFLAGS += -I/usr/include/openssl

volcano_SOURCES = volcano.c helper.c