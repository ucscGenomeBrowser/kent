include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslxToFa.o

pslxToFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslxToFa $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/pslxToFa
