include ../../../inc/makefile.h

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToDnaMotif.o ../lib/improbRunInfo.o

iriToDnaMotif: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/iriToDnaMotif $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/iriToDnaMotif
