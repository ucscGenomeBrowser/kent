.c.o:
	gcc -ggdb -O -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = goldToAgp.o

goldToAgp: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/goldToAgp $O $(MYLIBS) $L

lib:
	cd ../../lib; make
