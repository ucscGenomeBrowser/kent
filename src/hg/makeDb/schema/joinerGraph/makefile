include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = joinerGraph.o

joinerGraph: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/joinerGraph $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/joinerGraph
