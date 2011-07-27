SUBDIRS = libfilter newscope tests

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = opendiamond.pc

EXTRA_DIST = CREDITS LICENSE INSTALL \
	setup.py \
	pylintrc \
	opendiamond/__init__.py \
	opendiamond/bundle.py \
	opendiamond/config.py \
	opendiamond/helpers.py \
	opendiamond/scope.py \
	opendiamond/test.py \
	opendiamond/dataretriever/__init__.py \
	opendiamond/dataretriever/diamond_store.py \
	opendiamond/dataretriever/flickr_store.py \
	opendiamond/dataretriever/gigapan_store.py \
	opendiamond/dataretriever/mirage_store.py \
	opendiamond/dataretriever/proxy_store.py \
	opendiamond/dataretriever/pyramid.py \
	opendiamond/dataretriever/util.py \
	opendiamond/filter/__init__.py \
	opendiamond/filter/parameters.py \
	opendiamond/scopeserver/__init__.py \
	opendiamond/scopeserver/core/__init__.py \
	opendiamond/scopeserver/core/urls.py \
	opendiamond/scopeserver/core/views.py \
	opendiamond/scopeserver/core/static/scopeserver/base.css \
	opendiamond/scopeserver/core/static/scopeserver/jquery.js \
	opendiamond/scopeserver/core/static/scopeserver/style.css \
	opendiamond/scopeserver/core/templates/registration/login.html \
	opendiamond/scopeserver/core/templates/scopeserver/base.html \
	opendiamond/scopeserver/core/templates/scopeserver/home.html \
	opendiamond/scopeserver/core/templates/scopeserver/simple_form.html \
	opendiamond/scopeserver/flickr/__init__.py \
	opendiamond/scopeserver/flickr/forms.py \
	opendiamond/scopeserver/flickr/models.py \
	opendiamond/scopeserver/flickr/urls.py \
	opendiamond/scopeserver/flickr/views.py \
	opendiamond/scopeserver/gatekeeper/__init__.py \
	opendiamond/scopeserver/gatekeeper/admin.py \
	opendiamond/scopeserver/gatekeeper/forms.py \
	opendiamond/scopeserver/gatekeeper/models.py \
	opendiamond/scopeserver/gatekeeper/urls.py \
	opendiamond/scopeserver/gatekeeper/views.py \
	opendiamond/scopeserver/gatekeeper/templates/scopeserver/gatekeeper.html \
	opendiamond/scopeserver/gatekeeper/templates/scopeserver/gatekeeper_manage.html \
	opendiamond/scopeserver/gigapan/__init__.py \
	opendiamond/scopeserver/gigapan/forms.py \
	opendiamond/scopeserver/gigapan/models.py \
	opendiamond/scopeserver/gigapan/urls.py \
	opendiamond/scopeserver/gigapan/views.py \
	opendiamond/scopeserver/gigapan/templates/scopeserver/gigapan_browse.html \
	opendiamond/scopeserver/gigapan/templates/scopeserver/gigapan_search.html \
	opendiamond/scopeserver/mirage/__init__.py \
	opendiamond/scopeserver/mirage/forms.py \
	opendiamond/scopeserver/mirage/models.py \
	opendiamond/scopeserver/mirage/urls.py \
	opendiamond/scopeserver/mirage/views.py \
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
	tools/cookiecutter \
	tools/dataretriever \
	tools/diamondd \
	tools/diamond-bundle-search \
	tools/volcano

# Once pylint 0.24 becomes widely available, this should be replaced with
# "--ignore dataretriever --ignore scopeserver".
PYLINT_MODULES = \
	opendiamond.bundle \
	opendiamond.config \
	opendiamond.filter \
	opendiamond.helpers \
	opendiamond.scope \
	opendiamond.server \
	opendiamond.test

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

check-local:
	@# Fail on serious pylint errors
	PYTHONPATH=$(srcdir) \
		$(PYLINT) --rcfile=$(srcdir)/pylintrc $(PYLINT_MODULES) || \
		[ $$(($$? & 3)) -eq 0 ]
	PYTHONPATH=$(srcdir) PYTHONDONTWRITEBYTECODE=1 \
		$(PYTHON) $(srcdir)/opendiamond/test.py

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/opendiamond*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/opendiamond
	$(RM) $(DESTDIR)$(bindir)/cookiecutter
	$(RM) $(DESTDIR)$(bindir)/dataretriever
	$(RM) $(DESTDIR)$(bindir)/diamondd
	$(RM) $(DESTDIR)$(bindir)/diamond-bundle-search

clean-local:
	$(RM) -r build
