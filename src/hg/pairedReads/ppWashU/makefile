.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppWashU.o

ppWashU: $O $(MYLIBS)
	gcc -ggdb -o ppWashU $O $(MYLIBS) $L
	mv ppWashU $(HOME)/bin/$(MACHTYPE)
