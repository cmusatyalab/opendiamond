EXTRA_DIST = setup.py \
	opendiamond/__init__.py \
	opendiamond/bundle.py \
	opendiamond/config.py \
	opendiamond/helpers.py \
	opendiamond/scope.py \
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
	tools/cookiecutter \
	tools/diamondd \
	tools/diamond-bundle-python

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/opendiamond*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/opendiamond
	$(RM) $(DESTDIR)$(bindir)/diamond-bundle-python

clean-local:
	$(RM) -r build
