noinst_LTLIBRARIES = libscope.la
dist_bin_SCRIPTS = diamond-newscope

EXTRA_DIST = scope.desktop

install-exec-hook:
	desktop-file-install --rebuild-mime-info-cache --vendor diamond scope.desktop \
	|| desktop-file-install --dir $$HOME/.local/share/applications --rebuild-mime-info-cache --vendor diamond $(srcdir)/scope.desktop


libscope_la_SOURCES = lib_scope.c scope_adiskd.c scope_common.c

pkginclude_HEADERS = lib_scope.h scope_priv.h

cookiecutter_SOURCES = cookiecutter.c
noinst_PROGRAMS = cookiecutter


AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(GNUTLS_CFLAGS) $(UUID_CFLAGS)
AM_LDFLAGS = $(GLIB2_LIBS) $(GNUTLS_LIBS) $(UUID_LIBS)

