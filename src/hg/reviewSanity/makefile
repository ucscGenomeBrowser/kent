include ../../inc/makefile.h


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = reviewSanity.o

reviewSanity: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/reviewSanity $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
