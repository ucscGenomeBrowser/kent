include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faSomeRecords.o

faSomeRecords: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faSomeRecords $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faSomeRecords
