include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gpStats.o

gpStats: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gpStats $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gpStats


lib:
	cd ../lib; make
	cd ../../lib; make
