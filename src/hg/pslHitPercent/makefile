.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslHitPercent.o 

pslHitPercent: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslHitPercent $O $(MYLIBS) $L

lib:
	cd ../../lib; make
