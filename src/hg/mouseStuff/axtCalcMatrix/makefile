include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtCalcMatrix.o

axtCalcMatrix: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtCalcMatrix $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtCalcMatrix
