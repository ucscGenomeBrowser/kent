include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslCoverage.o 

pslCoverage: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslCoverage $O $(MYLIBS) $L

