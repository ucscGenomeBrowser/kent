.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = fragPart.o 

fragPart: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/fragPart $O $(MYLIBS) $L

lib:
	cd ../../lib; make
