.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtRecipBest.o

axtRecipBest: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtRecipBest $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/axtRecipBest

tags:  
	ctags *.c ../../../lib/*.c ../../../inc/*.h ../../lib/*.c ../../inc/*.h
