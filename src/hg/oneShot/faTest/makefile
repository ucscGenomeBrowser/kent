.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faTest.o

faTest: $O 
	gcc -o faTest $O $(MYLIBS) $L

