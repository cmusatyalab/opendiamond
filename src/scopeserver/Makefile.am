EXTRA_DIST = setup.py __init__.py opendiamond.py \
    templates/base.html templates/simple_form.html \
    flickr/__init__.py flickr/forms.py flickr/views.py \
    gatekeeper/__init__.py gatekeeper/admin.py gatekeeper/forms.py \
    gatekeeper/models.py gatekeeper/views.py \
    gatekeeper/templates/gatekeeper.html \
    gatekeeper/templates/gatekeeper_manage.html \
    vmfind/__init__.py vmfind/forms.py vmfind/views.py

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)
	
uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/scopeserver*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver
	
clean-local:
	$(RM) -r build

