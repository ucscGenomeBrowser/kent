.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgStsAlias.o

hgStsAlias: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgStsAlias $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
