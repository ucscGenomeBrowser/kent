include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sangPairs.o

sangPairs: $O 
	gcc -o sangPairs $O $(MYLIBS) $L

