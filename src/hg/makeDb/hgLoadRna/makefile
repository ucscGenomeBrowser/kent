include ../../../inc/makefile.h

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadRna.o 

hgLoadRna: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb ${F64} -o $(HOME)/bin/$(MACHTYPE)/hgLoadRna $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
