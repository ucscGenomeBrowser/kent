include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslSplit.o

pslSplit: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSplit $O $(MYLIBS) $L


lib:
	cd ../lib; make
	cd ../../lib; make
