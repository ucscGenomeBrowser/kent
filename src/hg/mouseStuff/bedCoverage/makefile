include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedCoverage.o

bedCoverage: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/bedCoverage $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/bedCoverage
