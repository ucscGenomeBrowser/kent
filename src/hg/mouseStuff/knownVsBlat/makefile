.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type-I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
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
