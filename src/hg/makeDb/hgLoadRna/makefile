include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = $(HOME)/bin/$(MACHTYPE)

O = hgLoadRna.o 

hgLoadRna: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb ${F64} -o $(INSTALLDIR)/hgLoadRna $O $(MYLIBS) $L

lib:
	cd ../../../lib; make

test: $(INSTALLDIR)/$(PROG)
	cd test && csh runtest

clean: 
	rm -f $(O)
