.c.o:
	gcc -ggdb -g -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFaIntoContigs.o

splitFaIntoContigs: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs $O $(MYLIBS) $L
	cp $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs .

clean:
	rm -f *.o $(HOME)/bin/$(MACHTYPE)/splitFaIntoContigs ./splitFaIntoContigs
