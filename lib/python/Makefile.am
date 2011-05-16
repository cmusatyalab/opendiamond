EXTRA_DIST = setup.py \
	opendiamond/__init__.py \
	opendiamond/bundle.py \
	opendiamond/helpers.py \
	opendiamond/scope.py \
	opendiamond/filter/__init__.py \
	opendiamond/filter/parameters.py \
	tools/cookiecutter \
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
