include ../../../inc/makefile.h

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = selectTrainedHits.o

selectTrainedHits: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/selectTrainedHits $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/selectTrainedHits
