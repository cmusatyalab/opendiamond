EXTRA_DIST = setup.py \
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
    scopeserver_vmfind/templates/simple_form.html

all-local: setup.py $(EXTRA_DIST)
	$(PYTHON) setup.py build

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)
	
uninstall-local:
	$(RM) $(DESTDIR)$(pythondir)/scopeserver*.egg-info
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_flickr
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_gatekeeper
	$(RM) -r $(DESTDIR)$(pythondir)/scopeserver_vmfind
	
clean-local:
	$(RM) -r build

