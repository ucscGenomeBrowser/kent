.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mendMap.o

mendMap: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/mendMap $O $(MYLIBS) $L

