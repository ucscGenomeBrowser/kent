include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgWaba.o 

hgWaba: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgWaba $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
