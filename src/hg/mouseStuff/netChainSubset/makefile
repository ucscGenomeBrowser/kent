.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = netChainSubset.o

netChainSubset: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/netChainSubset $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/netChainSubset
