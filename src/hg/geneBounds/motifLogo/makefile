include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifLogo.o

motifLogo: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/motifLogo $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/motifLogo${EXE}
