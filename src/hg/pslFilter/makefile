.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslFilter.o 

pslFilter: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslFilter $O $(MYLIBS) $L

lib:
	cd ../../lib; make
