# Makefile for project

TEXSRC=diamond_overview.tex 

all: diamond_overview.ps diamond_overview.pdf

clean:
	rm diamond_overview.ps diamond_overview.pdf diamond_overview.dvi diamond_overview.aux diamond_overview.bbl diamond_overview.blg diamond_overview.log


diamond_overview.pdf: diamond_overview.ps
	ps2pdf diamond_overview.ps

diamond_overview.ps: diamond_overview.dvi
	dvips -t letter -o diamond_overview.ps diamond_overview

diamond_overview.dvi: $(TEXSRC) 
	latex diamond_overview
	latex diamond_overview

diamond_overview.bbl: diamond-fast2004.bib $(TEXSRC)
	latex diamond-fast2004
	bibtex diamond-fast2004

