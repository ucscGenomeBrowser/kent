include ../inc/common.mk

L = -lm $(SOCKETLIB)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkOwnLib.a $(MYLIBDIR)/jkweb.a 

O = gfClient.o
X = gfClient

gfClient: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/$X $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/$X${EXE}
