.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSizes.o

ooCloneSizes: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooCloneSizes $O $(MYLIBS) $L

