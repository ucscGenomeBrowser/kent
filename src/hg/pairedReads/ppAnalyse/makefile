.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppAnalyse.o 

ppAnalyse: $O $(MYLIBS)
	gcc -ggdb -o ppAnalyse $O $(MYLIBS) $L
	mv ppAnalyse $(HOME)/bin/$(MACHTYPE)


