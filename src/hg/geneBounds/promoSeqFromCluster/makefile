include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = promoSeqFromCluster.o

promoSeqFromCluster: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/promoSeqFromCluster $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/promoSeqFromCluster
