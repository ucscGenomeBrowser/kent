.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslGenoJobs.o

pslGenoJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslGenoJobs $O $(MYLIBS) $L


fix: makeFix.o
	gcc -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	gcc -o test test.o $(MYLIBS) $L

