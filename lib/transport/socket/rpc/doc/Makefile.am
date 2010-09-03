DOCHEADERS   = $(top_srcdir)/minirpc/minirpc.h.in
DOCFILES     = main.dox common.dox example.dox
DOCFILES    += example_minirpc.h example_client.h example_server.h example_xdr.h
EXAMPLES     = example.mx example_client.c example_server.c
EXAMPLEPATHS = $(foreach file,$(EXAMPLES),$(top_srcdir)/example/$(file))
CONFFILES    = Doxyfile footer.html

SRCPATHS  = $(foreach file,$(DOCFILES),$(srcdir)/$(file)) $(DOCHEADERS)
EXTRA_DIST = $(CONFFILES) $(DOCFILES)

DOXYCMD = SRCFILES="$(SRCPATHS)" SRCPATH=$(top_srcdir) VERSION=$(VERSION) \
			$(DOXYGEN) $(srcdir)/Doxyfile >/dev/null

if HAVE_DOXYGEN
html.stamp: $(CONFFILES) $(SRCPATHS) $(EXAMPLEPATHS)
	rm -rf html
	GENERATE_HTML=YES GENERATE_LATEX=NO $(DOXYCMD)
	touch html.stamp

pdf.stamp: $(CONFFILES) $(SRCPATHS) $(EXAMPLEPATHS)
	rm -rf latex
	GENERATE_HTML=NO GENERATE_LATEX=YES $(DOXYCMD)
	$(MAKE) $(AM_MAKEFLAGS) -C latex pdf
	touch pdf.stamp
else
html.stamp:
	@echo "doxygen command not found.  Documentation will not be built."

pdf.stamp:
	@echo "doxygen command not found.  Documentation will not be built."
endif

html-local: html.stamp
pdf-local: pdf.stamp

installdirs-local:
	$(MKDIR_P) $(DESTDIR)$(htmldir) $(DESTDIR)$(pdfdir)

install-html-local: html-local installdirs-local
	-[ -d html ] && $(INSTALL_DATA) html/* $(DESTDIR)$(htmldir)

install-pdf-local: pdf-local installdirs-local
	-[ -d latex ] && $(INSTALL_DATA) latex/refman.pdf \
				$(DESTDIR)$(pdfdir)/manual.pdf

uninstall-local:
	rm -f $(DESTDIR)$(htmldir)/* $(DESTDIR)$(pdfdir)/manual.pdf

clean-local:
	rm -rf html html.stamp latex pdf.stamp
