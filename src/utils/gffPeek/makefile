include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gffPeek.o

gffPeek: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gffPeek $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/gffPeek${EXE}
