include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clonesInAgp.o

clonesInAgp: $O 
	gcc -o clonesInAgp $O $(MYLIBS) $L

