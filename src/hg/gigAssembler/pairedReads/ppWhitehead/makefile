.c.o:
	gcc -ggdb -O -Wimplicit -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppWhitehead.o 

ppWhitehead: $O $(MYLIBS)
	gcc -ggdb -o ppWhitehead $O $(MYLIBS) $L
	mv ppWhitehead $(HOME)/bin/$(MACHTYPE)

