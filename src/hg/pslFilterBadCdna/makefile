include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslFilterBadCdna.o

pslFilterBadCdna: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslFilterBadCdna $O $(MYLIBS) $L

lib:
	cd ../../lib; make
