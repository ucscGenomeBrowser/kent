.c.o:
	gcc -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslSortAcc.o

pslSortAcc: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSortAcc $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
