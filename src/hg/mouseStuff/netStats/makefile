include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netStats.o

netStats: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/netStats $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/netStats
