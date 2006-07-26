include ../../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGnfMicroarray.o

hgGnfMicroarray: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgGnfMicroarray $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgGnfMicroarray${EXE}

test:
	hgGnfMicroarray -tab=output -noLoad -limit=100 gnfHumanU95Exps gnfHumanU95All /projects/compbio/data/microarray/affyGnfHuman/data_public_U95
	diff -x CVS -r expected output
	rm -r output
