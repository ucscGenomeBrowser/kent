.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o

gfClient: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gfClient $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gfClient
