.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = $(EXPATLIB) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = t.o

t: $O $(MYLIBS)
	gcc -ggdb -o t $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/autoXml
