include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslSelfJobs.o

pslSelfJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSelfJobs $O $(MYLIBS) $L


lib:
	cd ../../lib; make
	cd ../../../lib; make
