bin_PROGRAMS = volcano

AM_CPPFLAGS += -I../../sqlite
volcano_LDADD = $(top_builddir)/lib/libopendiamond.la ../../sqlite/libsqlite.la

volcano_SOURCES = volcano.c helper.c helper.h
