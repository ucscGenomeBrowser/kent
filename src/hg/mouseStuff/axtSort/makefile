include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSort.o

axtSort: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtSort $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtSort
