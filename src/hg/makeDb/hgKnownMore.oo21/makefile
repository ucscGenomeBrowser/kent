.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgKnownMore.o hugoMulti.o

hgKnownMore: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgKnownMore $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
