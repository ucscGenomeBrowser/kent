include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitSim.o

splitSim: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/splitSim $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/splitSim${EXE}
