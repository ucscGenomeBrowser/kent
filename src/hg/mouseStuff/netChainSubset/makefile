include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netChainSubset.o

netChainSubset: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/netChainSubset $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/netChainSubset
