include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterGenes.o

clusterGenes: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/clusterGenes $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/clusterGenes
