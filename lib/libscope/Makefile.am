noinst_LTLIBRARIES = libscope.la
dist_bin_SCRIPTS = diamond-newscope
bin_PROGRAMS = cookiecutter
pkginclude_HEADERS = lib_scope.h

mimepkgdir = $(datarootdir)/mime/packages
dist_mimepkg_DATA = diamond-scope.xml
EXTRA_DIST = scope.desktop

libscope_la_SOURCES = lib_scope.c scope_adiskd.c scope_common.c scope_priv.h

cookiecutter_SOURCES = cookiecutter.c

install-data-hook:
	desktop-file-install --rebuild-mime-info-cache --vendor diamond $(srcdir)/scope.desktop \
	|| desktop-file-install --dir $$HOME/.local/share/applications --rebuild-mime-info-cache --vendor diamond $(srcdir)/scope.desktop
#	update-mime-database $(datarootdir)/mime

AM_CPPFLAGS += $(OPENDIAMOND_LIB_CPPFLAGS) $(GNUTLS_CFLAGS) $(UUID_CFLAGS)
AM_LDFLAGS = $(GLIB2_LIBS) $(GNUTLS_LIBS) $(UUID_LIBS)

