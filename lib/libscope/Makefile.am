dist_bin_SCRIPTS = diamond-newscope

mimepkgdir = $(datarootdir)/mime/packages
dist_mimepkg_DATA = diamond-scope.xml
EXTRA_DIST = scope.desktop

install-data-hook:
	desktop-file-install --rebuild-mime-info-cache --vendor diamond $(srcdir)/scope.desktop \
	|| desktop-file-install --dir $$HOME/.local/share/applications --rebuild-mime-info-cache --vendor diamond $(srcdir)/scope.desktop
#	update-mime-database $(datarootdir)/mime
