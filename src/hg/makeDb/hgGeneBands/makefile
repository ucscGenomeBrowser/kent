include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = hgGeneBands.o

hgGeneBands: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgGeneBands $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgGeneBands
