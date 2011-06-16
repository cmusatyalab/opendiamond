SUBDIRS = libfilter newscope tests

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = opendiamond.pc

EXTRA_DIST = CREDITS LICENSE INSTALL \
	setup.py \
	pylintrc \
	dataretriever/__init__.py \
	dataretriever/diamond_store.py \
	dataretriever/flickr_store.py \
	dataretriever/mirage_store.py \
	dataretriever/proxy_store.py \
	dataretriever/util.py \
	opendiamond/__init__.py \
	opendiamond/bundle.py \
	opendiamond/config.py \
	opendiamond/helpers.py \
	opendiamond/scope.py \
	opendiamond/test.py \
	opendiamond/filter/__init__.py \
	opendiamond/filter/parameters.py \
	opendiamond/server/__init__.py \
	opendiamond/server/blobcache.py \
	opendiamond/server/child.py \
	opendiamond/server/filter.py \
	opendiamond/server/listen.py \
	opendiamond/server/object_.py \
	opendiamond/server/protocol.py \
	opendiamond/server/rpc.py \
	opendiamond/server/scopelist.py \
	opendiamond/server/search.py \
	opendiamond/server/sessionvars.py \
	opendiamond/server/statistics.py \
	scopeserver_flickr/__init__.py \
	scopeserver_flickr/forms.py \
	scopeserver_flickr/views.py \
	scopeserver_flickr/templates/simple_form.html \
	scopeserver_gatekeeper/__init__.py \
	scopeserver_gatekeeper/admin.py \
	scopeserver_gatekeeper/forms.py \
	scopeserver_gatekeeper/models.py \
	scopeserver_gatekeeper/views.py \
	scopeserver_gatekeeper/templates/gatekeeper.html \
	scopeserver_gatekeeper/templates/gatekeeper_manage.html \
	scopeserver_vmfind/__init__.py \
	scopeserver_vmfind/forms.py \
	scopeserver_vmfind/views.py \
	scopeserver_vmfind/templates/simple_form.html \
	tools/cookiecutter \
	tools/dataretriever \
	tools/diamondd \
	tools/diamond-bundle-python \
	tools/volcano

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

check-local:
	@# Fail on serious pylint errors
	PYTHONPATH=$(srcdir) \
		$(PYLINT) --rcfile=$(srcdir)/pylintrc opendiamond || \
		[ $$(($$? & 3)) -eq 0 ]
	PYTHONPATH=$(srcdir) PYTHONDONTWRITEBYTECODE=1 \
		$(PYTHON) $(srcdir)/opendiamond/test.py

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/dataretriever*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/dataretriever
	$(RM) $(DESTDIR)$(pythondir)/opendiamond*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/opendiamond
	$(RM) $(DESTDIR)$(pythondir)/scopeserver*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_flickr
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_gatekeeper
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_vmfind
	$(RM) $(DESTDIR)$(bindir)/cookiecutter
	$(RM) $(DESTDIR)$(bindir)/dataretriever
	$(RM) $(DESTDIR)$(bindir)/diamondd
	$(RM) $(DESTDIR)$(bindir)/diamond-bundle-python

clean-local:
	$(RM) -r build
