include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgAvidShortBed.o

hgAvidShortBed: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgAvidShortBed $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgAvidShortBed
