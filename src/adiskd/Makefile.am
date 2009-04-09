bin_PROGRAMS = adiskd

adiskd_SOURCES = \
	adiskd.c search_state.c background.c search_state.h

AM_CPPFLAGS += $(LIBSOUP_CFLAGS)
adiskd_LDADD = $(top_builddir)/lib/libopendiamond.la ${GLIB2_LIBS}
