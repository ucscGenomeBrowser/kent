include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = aNotB.o

aNotB: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/aNotB $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/aNotB
