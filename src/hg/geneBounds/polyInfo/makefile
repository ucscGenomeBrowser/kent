include ../../../inc/common.mk

L = -lm  $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = polyInfo.o ../lib/estInfo.o

polyInfo: $O $(MYLIBS) makefile
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/polyInfo $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/polyInfo
