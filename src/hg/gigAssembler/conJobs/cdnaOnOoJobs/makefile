include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cdnaOnOoJobs.o

cdnaOnOoJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/cdnaOnOoJobs $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/cdnaOnOoJobs

fix: makeFix.o
	gcc -o makeFix makeFix.o $(MYLIBS) $L

test: test.o
	gcc -o test test.o $(MYLIBS) $L

