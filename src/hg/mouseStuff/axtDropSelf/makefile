include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtDropSelf.o

axtDropSelf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtDropSelf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtDropSelf
