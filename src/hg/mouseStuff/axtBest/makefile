include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBest.o

axtBest: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtBest $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtBest
