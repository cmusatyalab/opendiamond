bin_SCRIPTS = minirpcgen
CLEANFILES = minirpcgen

SUBDIRS = . minirpc lib test
EXTRA_DIST = minirpcgen.in

minirpcgen: minirpcgen.in Makefile
	@echo "Generating $@ from $<"
	@sed -e "s=!!RPCGEN!!=$(RPCGEN)=g" \
				-e "s=!!PERLPATH!!=$(PERL)=g" $< > $@
	@chmod +x $@
