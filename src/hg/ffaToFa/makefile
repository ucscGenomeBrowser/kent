include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ffaToFa.o

ffaToFa: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/ffaToFa $O $(MYLIBS) $L

lib:
	cd ../../lib; make
