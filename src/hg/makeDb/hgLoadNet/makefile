.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadNet.o 

hgLoadNet: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgLoadNet $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
