.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRefAlign.o hgRefAlignWebb.o

hgTraceInfo: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgRefAlign $O $(MYLIBS) $L

clean:
	rm -f ${O}

lib:
	cd ../../../lib; make

hgRefAlignWebb.o: hgRefAlignWebb.h
hgRefAlign.o: hgRefAlignWebb.h
