.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslSort.o

pslSort: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSort $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
