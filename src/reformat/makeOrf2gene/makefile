.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = makeOrf2gene.o

makeOrf2gene: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/makeOrf2gene $O $(MYLIBS) $L
