.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clustAlt.o hgapIn.o

clustAlt: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/clustAlt $O ../../lib/$(MACHTYPE)/jkweb.a $(L)

lib:
	cd ../../lib; make
