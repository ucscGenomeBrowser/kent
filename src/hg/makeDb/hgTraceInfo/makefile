.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTraceInfo.o

hgTraceInfo: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgTraceInfo $O $(MYLIBS) $L

clean:
	rm -f ${O}

lib:
	cd ../../../lib; make
