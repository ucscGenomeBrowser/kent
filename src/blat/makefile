.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

L = -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = blat.o

blat: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/blat $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/blat

tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h
