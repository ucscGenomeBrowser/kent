include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a 

O = axtChain.o

axtChain: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtChain $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtChain
