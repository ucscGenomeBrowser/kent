include ../../../inc/common.mk

L = -lm  $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgClusterGenes.o

hgClusterGenes: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgClusterGenes $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgClusterGenes${EXE}
