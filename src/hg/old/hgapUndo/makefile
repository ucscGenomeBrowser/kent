.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgapUndo.o 

hgapUndo: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgapUndo $O $(MYLIBS) $L

lib:
	cd ../../lib; make
