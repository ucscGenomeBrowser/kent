.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = wikiPlot.o

wikiPlot: $O $(MYLIBS)
	gcc $O $(MYLIBS) $L
	mv a.out wikiPlot
	strip wikiPlot
	chmod a+rx wikiPlot
	mv wikiPlot $(HGCGI)
