include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = makePai.o 

makePai: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/makePai $O $(MYLIBS) $L

lib:
	cd ../../lib; make
