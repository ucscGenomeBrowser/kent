.c.o:
	gcc -O -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sortFilt.o

sortFilt: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/sortFilt $O $(MYLIBS) $L

lib:
	cd ../../lib; make
