include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fixEnsGtf.o

fixEnsGtf: $O 
	gcc -o fixEnsGtf $O $(MYLIBS) $L

