include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslMrnaCover.o

pslMrnaCover: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslMrnaCover $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/pslMrnaCover
