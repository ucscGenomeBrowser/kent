include ../inc/common.mk

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o
X = gfClient.27x1

gfClient: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/$X $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/$X
