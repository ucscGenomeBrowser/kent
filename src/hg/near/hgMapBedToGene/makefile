include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgMapBedToGene.o

hgMapBedToGene: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgMapBedToGene $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgMapBedToGene
