include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftPromoHits.o

liftPromoHits: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/liftPromoHits $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/liftPromoHits
