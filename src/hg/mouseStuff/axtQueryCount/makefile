include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtQueryCount.o

axtQueryCount: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtQueryCount $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtQueryCount
