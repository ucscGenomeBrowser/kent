include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = promoSeqFromCluster.o

promoSeqFromCluster: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/promoSeqFromCluster $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/promoSeqFromCluster
