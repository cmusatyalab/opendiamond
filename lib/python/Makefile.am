EXTRA_DIST = setup.py \
	opendiamond/__init__.py \
	opendiamond/helpers.py

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/opendiamond*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/opendiamond

clean-local:
	$(RM) -r build
