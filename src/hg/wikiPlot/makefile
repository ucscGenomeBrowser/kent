include ../../inc/common.mk


L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = wikiPlot.o

wikiPlot: $O $(MYLIBS)
	gcc $O $(MYLIBS) $L
	mv a.out wikiPlot
	${STRIP} wikiPlot${EXE}
	chmod a+rx wikiPlot
	mv wikiPlot $(HGCGI)
