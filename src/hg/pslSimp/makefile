include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslSimp.o 

pslSimp: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSimp $O $(MYLIBS) $L

lib:
	cd ../../lib; make
