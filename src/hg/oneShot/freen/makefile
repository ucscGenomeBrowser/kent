include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = freen.o

hello: freen.o
	gcc -o $(HOME)/bin/$(MACHTYPE)/freen $O $(MYLIBS) $L

