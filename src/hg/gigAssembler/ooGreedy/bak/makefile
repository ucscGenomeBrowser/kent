.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooGreedy.o

ooGreedy.84:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c ooGreedy.c
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/ooGreedy.84 ooGreedy.o $(MYLIBS) $L

lib:
	cd ../lib; make
	cd ../../lib; make
