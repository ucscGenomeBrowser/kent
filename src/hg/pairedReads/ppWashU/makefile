.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/jkhgap.a

O = ppWashU.o

ppWashU: $O $(MYLIBS)
	gcc -ggdb -o ppWashU $O $(MYLIBS) $L
	mv ppWashU $(HOME)/bin/$(MACHTYPE)
