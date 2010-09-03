GEN = minirpc.h
CLEANFILES = $(GEN)
EXTRA_DIST = $(GEN:=.in)

nodist_pkginclude_HEADERS = $(GEN)
pkginclude_HEADERS = protocol.h

$(GEN): %: %.in Makefile
	@echo "Generating $@ from $<"
	@sed -e "s=!!VERSION_CODE!!=$(VERSION_CODE)=g" $< > $@
