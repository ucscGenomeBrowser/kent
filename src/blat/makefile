.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = blat.o blastOut.o

blat: $O $(MYLIBS)
	gcc -ggdb -o ../../bin/$(MACHTYPE)/blat.14 $O $(MYLIBS) $L
	strip ../../bin/$(MACHTYPE)/blat.14

all:
	cd ../lib; make
	make
tags:
	ctags *.c *.h ../lib/*.c ../inc/*.h
