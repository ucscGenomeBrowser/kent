include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgWormLinks.o

hgWormLinks: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgWormLinks $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgWormLinks
