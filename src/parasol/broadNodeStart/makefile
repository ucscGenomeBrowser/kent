include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadNodeStart.o

broadNodeStart: $O $(MYLIBS)
	gcc -ggdb -o ../bin/broadNodeStart $O $(MYLIBS) $L
	strip ../bin/broadNodeStart
