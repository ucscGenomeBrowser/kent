include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = testXap.o

testXap: $O $(MYLIBS)
	gcc -ggdb -o testXap $O $(MYLIBS) $L
	strip testXap
