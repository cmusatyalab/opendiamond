# Makefile for project

TEXSRC=diamond_design.tex 

all: diamond_design.ps diamond_design.pdf

clean:
	rm diamond_design.ps diamond_design.pdf diamond_design.dvi diamond_design.aux diamond_design.bbl diamond_design.blg diamond_design.log


diamond_design.pdf: diamond_design.ps
	ps2pdf diamond_design.ps

diamond_design.ps: diamond_design.dvi
	dvips -t letter -o diamond_design.ps diamond_design

diamond_design.dvi: $(TEXSRC) 
	latex diamond_design
	latex diamond_design

diamond_design.bbl: diamond-fast2004.bib $(TEXSRC)
	latex diamond-fast2004
	bibtex diamond-fast2004

