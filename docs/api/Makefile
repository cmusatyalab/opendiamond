all: latex/refman.tex 


latex/refman.tex:  doxygen_config title.c
	doxygen doxygen_config

clean:
	rm -f *.ps *.dvi *.aux *.toc *.idx *.ind *.ilg *.log *.out refman.pdf
