include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgProtIdToGenePred.o

hgProtIdToGenePred: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgProtIdToGenePred $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgProtIdToGenePred
