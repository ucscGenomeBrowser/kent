include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslSort.o

pslSort: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSort $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
