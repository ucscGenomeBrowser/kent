.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cartReset.o

cartReset: $O $(MYLIBS)
	gcc -ggdb -o cartReset $O $(MYLIBS) $L
	strip cartReset
	chmod a+rx cartReset
	mv cartReset $(HGCGI)
