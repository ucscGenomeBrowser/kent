include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = utrFa.o

utrFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/utrFa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/utrFa
