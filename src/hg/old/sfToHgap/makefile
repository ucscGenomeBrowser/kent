.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sfToHgap.o mrnaToContig.o

sfToHgap: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/sfToHgap $O $(MYLIBS) $L

lib:
	cd ../../lib; make
