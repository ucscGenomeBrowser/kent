.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = evalEndOverlaps.o ocp.o

evalEndOverlaps: $O 
	gcc -o evalEndOverlaps $O $(MYLIBS) $L

