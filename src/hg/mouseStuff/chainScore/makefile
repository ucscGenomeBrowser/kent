include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainScore.o

chainScore: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainScore $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chainScore${EXE}
