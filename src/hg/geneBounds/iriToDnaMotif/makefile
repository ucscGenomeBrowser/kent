include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = iriToDnaMotif.o ../lib/improbRunInfo.o

iriToDnaMotif: $O $(MYLIBS)
	cd ../lib && ${MAKE}
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/iriToDnaMotif $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/iriToDnaMotif
