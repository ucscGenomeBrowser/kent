include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafToPsl.o

mafToPsl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mafToPsl $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/mafToPsl
