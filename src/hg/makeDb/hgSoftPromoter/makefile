include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSoftPromoter.o

hgSoftPromoter: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgSoftPromoter $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgSoftPromoter
