include ../../inc/common.mk


L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = wikiPlot.o

wikiPlot: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $L
	mv a.out wikiPlot
	${STRIP} wikiPlot${EXE}
	chmod a+rx wikiPlot
	mv wikiPlot $(HGCGI)
