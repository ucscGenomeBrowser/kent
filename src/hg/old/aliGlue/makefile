.c.o:
	gcc -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = aliGlue.o

aliGlue: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/aliGlue $O $(MYLIBS) $L

lib:
	cd ../../lib; make
