include ../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = genePredToGtf.o

genePredToGtf: $O $(MYLIBS) makefile
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/genePredToGtf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/genePredToGtf
