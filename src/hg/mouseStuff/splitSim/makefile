include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitSim.o

splitSim: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/splitSim $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/splitSim
