.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gbOneAcc.o 

gbOneAcc: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gbOneAcc $O $(MYLIBS) $L

lib:
	cd ../../lib; make
