include ../../../inc/common.mk

L = -lm  $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgClusterGenes.o

hgClusterGenes: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgClusterGenes $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/hgClusterGenes
