.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a


O = pslReps.o 

pslReps: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslReps $O $(MYLIBS) $L

lib:
	cd ../../lib; make
