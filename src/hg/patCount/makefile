.c.o:
	gcc -O -Wimplicit -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patCount.o

patCount: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/patCount $O $(MYLIBS) $L

lib:
	cd ../../lib; make
