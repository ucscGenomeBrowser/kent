include ../inc/makefile.h

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfServer.o

gfServer: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/gfServer $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/gfServer
