include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterGenes.o

clusterGenes: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/clusterGenes $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/clusterGenes
