include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = jabba.o jobDb.o

jabba: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/jabba $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/jabba
