include ../../../inc/makefile.h

L = -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = lavToPsl.o

lavToPsl: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/lavToPsl $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/lavToPsl
