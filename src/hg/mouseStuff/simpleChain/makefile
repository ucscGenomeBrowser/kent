include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = simpleChain.o

simpleChain: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/simpleChain $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/simpleChain
