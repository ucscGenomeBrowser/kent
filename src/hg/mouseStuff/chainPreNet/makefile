include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainPreNet.o

chainPreNet: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainPreNet $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/chainPreNet
