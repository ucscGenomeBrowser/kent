include ../../inc/makefile.h


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
