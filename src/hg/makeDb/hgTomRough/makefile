.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTomRough.o tomRough.o

hgTomRough: $O 
	gcc -ggdb -o hgTomRough $O $(MYLIBS) $L

lib:
	cd ../../../lib; make
