include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sanger22gtf.o

sanger22gtf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/sanger22gtf $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/sanger22gtf
