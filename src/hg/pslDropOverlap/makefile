include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslDropOverlap.o 

pslDropOverlap: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslDropOverlap $O $(MYLIBS) $L

lib:
	cd ../../lib; make
