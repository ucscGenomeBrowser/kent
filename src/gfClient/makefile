include ../inc/makefile.h

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o

gfClient: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gfClient $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gfClient
