include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faSimplify.o

faSimplify: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faSimplify $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faSimplify
