include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

S = blatStats.o

O = knownVsBlat.o $S

all: knownVsBlat kvsSummary

knownVsBlat: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/knownVsBlat $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/knownVsBlat 

O2 = kvsSummary.o $S

kvsSummary: $(O2) $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/kvsSummary $(O2) $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/kvsSummary 
