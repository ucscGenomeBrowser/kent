include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGnfMicroarray.o

hgGnfMicroarray: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgGnfMicroarray $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgGnfMicroarray

test:
	hgGnfMicroarray -tab=output -noLoad -limit=100 gnfHumanU95Exps gnfHumanU95All /projects/compbio/data/microarray/affyGnfHuman/data_public_U95
	diff -x CVS -r expected output
	rm -r output
