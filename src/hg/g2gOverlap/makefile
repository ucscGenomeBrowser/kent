.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = g2gOverlap.o 

g2gOverlap: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/g2gOverlap $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/g2gOverlap

lib:
	cd ../../lib; make
