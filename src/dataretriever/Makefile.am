EXTRA_DIST = setup.py dataretriever __init__.py util.py \
     diamond_store.py mirage_store.py

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)
	
uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/dataretriever*.egg-info
	$(RM) $(DESTDIR)$(bindir)/dataretriever
	$(RM) -r $(DESTDIR)$(pythondir)/dataretriever
	
clean-local:
	$(RM) -r build

