.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = psLayout.o repMask.o

psLayout: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/psLayout $O $(MYLIBS) $L

chkSym: chkSym.o psLayHit.o ../../lib/$(MACHTYPE)/jkweb.a
	gcc -o chkSym chkSym.o psLayHit.o ../../lib/$(MACHTYPE)/jkweb.a $L

lib:
	cd ../../lib; make
