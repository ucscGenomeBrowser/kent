.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkEnds.o

checkEnds: $O 
	gcc -o checkEnds $O $(MYLIBS) $L

