.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cmParse.o imreClone.o

cmParse: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o cmParse $O $(MYLIBS) $L

lib:
	cd ../../lib; make
