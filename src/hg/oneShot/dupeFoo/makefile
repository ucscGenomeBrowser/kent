include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = dupeFoo.o

dupeFoo: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/dupeFoo $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/dupeFoo
