include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rnaFoldBig.o

rnaFoldBig: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/rnaFoldBig $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/rnaFoldBig
