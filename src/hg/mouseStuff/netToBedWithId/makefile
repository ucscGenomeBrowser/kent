include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netToBedWithId.o

netToBedWithId: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/netToBedWithId $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/netToBedWithId
