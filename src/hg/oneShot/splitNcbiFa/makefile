include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitNcbiFa.o

splitNcbiFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/splitNcbiFa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/splitNcbiFa
