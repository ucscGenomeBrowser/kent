.c.o:
	gcc -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pscReps.o ../pscSort/as_psc.o

pscReps: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/pscReps $O $(MYLIBS) $L

lib:
	cd ../../lib; make
