.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cmParse.o

cmParse: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o cmParse $O $(MYLIBS) $L

lib:
	cd ../../lib; make
