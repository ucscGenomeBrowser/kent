include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cmpOverlaps.o

cmpOverlaps: $O 
	gcc -o cmpOverlaps $O $(MYLIBS) $L

