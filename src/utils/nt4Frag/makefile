.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = nt4Frag.o

nt4Frag: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/nt4Frag $O $(MYLIBS) $L
