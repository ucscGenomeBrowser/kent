.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -c $*.c

L = -lm $(SOCKETLIB)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = gfClient.o

gfClient: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gfClient $O $(MYLIBS) $L
