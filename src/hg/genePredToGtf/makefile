include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
L = $(MYSQLLIBS) -lm
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredToGtf.o

genePredToGtf: $O $(MYLIBS) makefile
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/genePredToGtf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/genePredToGtf${EXE}
