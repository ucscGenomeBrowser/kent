.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gtfTab.o

gtfTab: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o gtfTab $O $(MYLIBS) $L

lib:
	cd ../../lib; make
