.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = cartDump.o

cartDump: $O $(MYLIBS)
	gcc -ggdb -o cartDump $O $(MYLIBS) $L
	strip cartDump
	chmod a+rx cartDump
	mv cartDump $(HGCGI)
