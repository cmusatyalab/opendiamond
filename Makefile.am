SUBDIRS = libfilter newscope tests

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = opendiamond.pc

nobase_pyexec_PYTHON = \
	opendiamond/attributes.py \
	opendiamond/blobcache.py \
	opendiamond/bundle.py \
	opendiamond/config.py \
	opendiamond/helpers.py \
	opendiamond/protocol.py \
	opendiamond/rpc.py \
	opendiamond/scope.py \
	opendiamond/test.py \
	opendiamond/xdr.py \
	opendiamond/blaster/__init__.py \
	opendiamond/blaster/cache.py \
	opendiamond/blaster/handlers.py \
	opendiamond/blaster/json.py \
	opendiamond/blaster/rpc.py \
	opendiamond/blaster/search.py \
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
	opendiamond/scopeserver/gigapan/__init__.py \
	opendiamond/scopeserver/gigapan/forms.py \
	opendiamond/scopeserver/gigapan/models.py \
	opendiamond/scopeserver/gigapan/urls.py \
	opendiamond/scopeserver/gigapan/views.py \
	opendiamond/scopeserver/mirage/__init__.py \
	opendiamond/scopeserver/mirage/forms.py \
	opendiamond/scopeserver/mirage/models.py \
	opendiamond/scopeserver/mirage/urls.py \
	opendiamond/scopeserver/mirage/views.py \
	opendiamond/server/__init__.py \
	opendiamond/server/child.py \
	opendiamond/server/filter.py \
	opendiamond/server/listen.py \
	opendiamond/server/object_.py \
	opendiamond/server/scopelist.py \
	opendiamond/server/search.py \
	opendiamond/server/sessionvars.py \
	opendiamond/server/statistics.py

noinst_PYTHON = \
	opendiamond/__init__.py \
	opendiamond/hash.py

nobase_dist_pyexec_DATA = \
	opendiamond/blaster/static/json-blaster.js \
	opendiamond/blaster/static/json-blaster-autopause.js \
	opendiamond/blaster/static/sockjs.js \
	opendiamond/blaster/static/testui/jquery.js \
	opendiamond/blaster/static/testui/spinner.gif \
	opendiamond/blaster/static/testui/unknown.png \
	opendiamond/blaster/templates/testui/result.html \
	opendiamond/blaster/templates/testui/results.html \
	opendiamond/blaster/templates/testui/search.html \
	opendiamond/scopeserver/core/static/scopeserver/base.css \
	opendiamond/scopeserver/core/static/scopeserver/jquery.js \
	opendiamond/scopeserver/core/static/scopeserver/style.css \
	opendiamond/scopeserver/core/templates/registration/login.html \
	opendiamond/scopeserver/core/templates/scopeserver/base.html \
	opendiamond/scopeserver/core/templates/scopeserver/home.html \
	opendiamond/scopeserver/core/templates/scopeserver/simple_form.html \
	opendiamond/scopeserver/gatekeeper/templates/scopeserver/gatekeeper.html \
	opendiamond/scopeserver/gatekeeper/templates/scopeserver/manage.html \
	opendiamond/scopeserver/gigapan/templates/scopeserver/gigapan_browse.html \
	opendiamond/scopeserver/gigapan/templates/scopeserver/gigapan_search.html

dist_pkgpyexec_DATA = \
	bundle/bundle.xsd

dist_bin_SCRIPTS = \
	tools/blaster \
	tools/cookiecutter \
	tools/dataretriever \
	tools/diamondd \
	tools/diamond-bundle-predicate

nobase_pyexec_LTLIBRARIES = opendiamond/hash.la
opendiamond_hash_la_SOURCES = opendiamond/hashmodule.c
opendiamond_hash_la_CFLAGS = $(PYTHON_CFLAGS)
opendiamond_hash_la_LDFLAGS = -avoid-version -module

dist_doc_DATA = doc/diamond-protocol.html
MAINTAINERCLEANFILES = doc/diamond-protocol.html

EXTRA_DIST = CREDITS LICENSE INSTALL \
	pylintrc \
	bundle/bundle.xslt \
	doc/diamond-protocol.xml \
	tools/volcano

doc/diamond-protocol.html: doc/diamond-protocol.xml
	$(MKDIR_P) doc
	$(XML2RFC) --html -o $@ $^

check-local:
	@# Fail on serious pylint errors
	@# Ensure the cwd doesn't have an opendiamond directory for pylint
	@# to choke on
	cd tests && \
		PYTHONPATH=$(abs_srcdir) \
		$(PYLINT) --rcfile=$(abs_srcdir)/pylintrc \
		--ignore dataretriever,scopeserver opendiamond || \
		[ $$(($$? & 3)) -eq 0 ]
	PYTHONPATH=$(srcdir) PYTHONDONTWRITEBYTECODE=1 \
		$(PYTHON) $(srcdir)/opendiamond/test.py

install-exec-local:
	@# Fabricate opendiamond/__init__.py containing the software version
	$(MKDIR_P) $(DESTDIR)/$(pkgpyexecdir)
	echo "__version__ = '$(VERSION)'" > $(DESTDIR)$(pkgpyexecdir)/__init__.py
	@# Set the __init__.py utimes to those of configure.ac to prevent
	@# file conflicts when installing both 32-bit and 64-bit RPM packages.
	@# Using configure.ac as the source file ensures that __init__.pyc
	@# is updated when the software version changes.
	touch -r $(top_srcdir)/configure.ac $(DESTDIR)$(pkgpyexecdir)/__init__.py

install-exec-hook:
	rm -f $(DESTDIR)$(pkgpyexecdir)/hash.la \
		$(DESTDIR)$(pkgpyexecdir)/hash.a

uninstall-hook:
	rm -f $(DESTDIR)$(pkgpyexecdir)/__init__.py \
		$(DESTDIR)$(pkgpyexecdir)/hash.so
