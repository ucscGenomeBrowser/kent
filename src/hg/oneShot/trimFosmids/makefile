include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = trimFosmids.o

trimFosmids: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/trimFosmids $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/trimFosmids
