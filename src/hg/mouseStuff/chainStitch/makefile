include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainStitch.o

chainStitch: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainStitch $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/chainStitch
