.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslSelfJobs.o

pslSelfJobs: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/pslSelfJobs $O $(MYLIBS) $L


lib:
	cd ../../lib; make
	cd ../../../lib; make
