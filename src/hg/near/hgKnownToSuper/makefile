include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgKnownToSuper.o

hgKnownToSuper: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgKnownToSuper $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgKnownToSuper
