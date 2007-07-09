bin_PROGRAMS = adiskd

CFLAGS += ${GLIB2_CFLAGS}

adiskd_SOURCES = \
	adiskd.c search_state.c background.c search_state.h

adiskd_LDADD = $(top_builddir)/lib/libopendiamond.la ${GLIB2_LIBS}
