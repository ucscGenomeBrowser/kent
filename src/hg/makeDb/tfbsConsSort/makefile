include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tfbsConsSort.o

tfbsConsSort: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/tfbsConsSort $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/tfbsConsSort
