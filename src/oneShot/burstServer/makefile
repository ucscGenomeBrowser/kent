include ../../inc/common.mk

L = -lm -lpthread
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = burstServer.o

burstServer: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/burstServer $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/burstServer
