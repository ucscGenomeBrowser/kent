include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netSyntenic.o

netSyntenic: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/netSyntenic $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/netSyntenic
