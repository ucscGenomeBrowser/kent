include ../../inc/common.mk

L = -lm -lsocket -lresolv
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = dnsInfo.o

dnsInfo: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/dnsInfo $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/dnsInfo${EXE}
