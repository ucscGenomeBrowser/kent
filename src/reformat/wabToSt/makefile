.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = wabToSt.o

wabToSt: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/wabToSt $O $(MYLIBS) $L
