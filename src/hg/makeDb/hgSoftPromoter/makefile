include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSoftPromoter.o

hgSoftPromoter: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgSoftPromoter $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgSoftPromoter
