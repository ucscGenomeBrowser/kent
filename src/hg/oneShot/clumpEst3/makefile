include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clumpEst3.o

clumpEst3: $O 
	gcc -o clumpEst3 $O $(MYLIBS) $L

