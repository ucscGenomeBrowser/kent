include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifSig.o ../lib/improbRunInfo.o

motifSig: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/motifSig $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/motifSig
