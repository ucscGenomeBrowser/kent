.c.o:
	gcc -ggdb -O -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ctgFaToFa.o

ctgFaToFa: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/ctgFaToFa $O $(MYLIBS) $L

lib:
	cd ../../lib; make
