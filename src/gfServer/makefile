.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfServer.o

gfServer: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gfServer $O $(MYLIBS) $L
