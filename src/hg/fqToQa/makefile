.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fqToQa.o

fqToQa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/fqToQa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/fqToQa


lib:
	cd ../lib; make
	cd ../../lib; make
