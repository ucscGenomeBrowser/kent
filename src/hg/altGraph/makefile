.c.o:
	gcc -O -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O =  altGraph.o 

TESTO = test.o

altGraph: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/altGraph $O $(MYLIBS) $L

test: $(TESTO)
	gcc -o test $(TESTO) $(MYLIBS) $L

lib:
	cd ../../lib; make
