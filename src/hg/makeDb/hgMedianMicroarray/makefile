include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgMedianMicroarray.o

hgMedianMicroarray: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgMedianMicroarray $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgMedianMicroarray

test:
	hgMedianMicroarray ce1 kimLifeCycle kimWormLifeCycle kimMed.ra kimLifeCycleMedian kimWormLifeCycleMedian -tab=output -noLoad -limit=100
	diff -r expected output
	rm -r output
