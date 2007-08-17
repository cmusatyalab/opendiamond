bin_PROGRAMS = volcano

volcano_LDADD = $(top_builddir)/lib/libopendiamond.la  ${SQLITE3_LIBS}
AM_CPPFLAGS += -I/usr/include/openssl

volcano_SOURCES = volcano.c helper.c