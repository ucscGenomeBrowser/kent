include ../../../inc/makefile.h

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainNet.o

chainNet: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainNet $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/chainNet
