include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastToAxt.o

blastToAxt: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/blastToAxt $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/blastToAxt
