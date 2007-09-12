noinst_LTLIBRARIES = libscope.la
dist_bin_SCRIPTS = newscope

install-exec-hook:
	desktop-file-install --rebuild-mime-info-cache --vendor diamond scope.desktop

libscope_la_SOURCES = lib_scope.c

pkginclude_HEADERS = lib_scope.h
