include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslOoJobs.o

pslOoJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslOoJobs $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/pslOoJobs

