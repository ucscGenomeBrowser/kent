include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFilter.o

mafFilter: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mafFilter $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/mafFilter${EXE}
