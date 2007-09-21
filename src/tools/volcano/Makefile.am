bin_PROGRAMS = volcano

volcano_LDADD = $(top_builddir)/lib/libopendiamond.la  ${SQLITE3_LIBS}
AM_CPPFLAGS += ${SQLITE3_CFLAGS}

volcano_SOURCES = volcano.c helper.c helper.h
