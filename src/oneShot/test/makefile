include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = test.o

test: $O $(MYLIBS)
	gcc -o test $O $(MYLIBS) $L
	strip test
