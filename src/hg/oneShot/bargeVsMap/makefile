.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bargeVsMap.o

bargeVsMap: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/bargeVsMap $O $(MYLIBS) $L

