.c.o:
	gcc -ggdb -O -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rmskOoJobs.o

rmskOoJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/rmskOoJobs $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/rmskOoJobs

fix: makeFix.o
	gcc -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	gcc -o test test.o $(MYLIBS) $L

