.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslIntronsOnly.o 

pslIntronsOnly: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslIntronsOnly $O $(MYLIBS) $L

lib:
	cd ../../lib; make
