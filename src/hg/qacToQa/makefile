include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = qacToQa.o

qacToQa: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/qacToQa $O $(MYLIBS) $L

lib:
	cd ../../lib; make
