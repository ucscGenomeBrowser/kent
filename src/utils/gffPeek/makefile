include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gffPeek.o

gffPeek: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gffPeek $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gffPeek
