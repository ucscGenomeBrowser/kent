include ../../../inc/makefile.h

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faFiltMasked.o

faFiltMasked: $O 
	gcc -o faFiltMasked $O $(MYLIBS) $L

