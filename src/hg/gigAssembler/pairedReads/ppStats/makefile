include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppStats.o pPair.o

ppStats: $O $(MYLIBS)
	gcc -ggdb -o ppStats $O $(MYLIBS) $L
	mv ppStats $(HOME)/bin/$(MACHTYPE)


lib:
	cd ../../lib; make
	cd ../../../lib; make
