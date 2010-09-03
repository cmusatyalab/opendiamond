bin_SCRIPTS = minirpcgen
CLEANFILES = minirpcgen

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = minirpc.pc

SUBDIRS = . minirpc lib test
EXTRA_DIST = COPYING minirpcgen.in

minirpcgen: minirpcgen.in Makefile
	@echo "Generating $@ from $<"
	@sed -e "s=!!RPCGEN!!=$(RPCGEN)=g" \
				-e "s=!!PERLPATH!!=$(PERL)=g" \
				-e "s=!!VERSION_CODE!!=$(VERSION_CODE)=g" \
				-e "s=!!VERSION!!=$(PACKAGE_VERSION)=g" $< > $@
	@chmod +x $@
