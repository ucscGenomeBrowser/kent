include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = phToPsl.o

phToPsl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/phToPsl $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/phToPsl
