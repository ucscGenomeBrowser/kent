include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgEmblProtLinks.o

hgEmblProtLinks: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgEmblProtLinks $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgEmblProtLinks
