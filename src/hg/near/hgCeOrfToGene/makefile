include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgCeOrfToGene.o

hgCeOrfToGene: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgCeOrfToGene $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgCeOrfToGene
