include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadNodeStart.o

broadNodeStart: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/broadNodeStart $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/broadNodeStart
