.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSpan.o

ooCloneSpan: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooCloneSpan $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/ooCloneSpan

