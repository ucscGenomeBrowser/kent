include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtSplitByTarget.o

axtSplitByTarget: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtSplitByTarget $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtSplitByTarget
