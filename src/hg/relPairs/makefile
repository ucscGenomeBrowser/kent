include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = relPairs.o 

relPairs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/relPairs $O $(MYLIBS) $L

lib:
	cd ../../lib; make
