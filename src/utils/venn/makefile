.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = venn.o

venn: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/venn $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/venn
