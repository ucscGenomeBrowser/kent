include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgStanfordMicroarray.o

hgStanfordMicroarray: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgStanfordMicroarray $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgStanfordMicroarray
