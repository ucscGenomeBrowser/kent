include ../../../inc/common.mk

L = $(EXPATLIB) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = t.o

t: $O $(MYLIBS)
	gcc -ggdb -o t $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/autoXml
