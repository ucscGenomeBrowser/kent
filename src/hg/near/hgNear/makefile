include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
X = hgNear

O = $X.o advSearch.o configure.o expRatio.o getSeq.o \
	gifLabel.o knownGene.o search.o

my: compile
	mv $X /usr/local/apache/cgi-bin-$(USER)/$X
	rsync -ar --delete hgNearData /usr/local/apache/cgi-bin-$(USER)/

alpha: compile
	mv $X /usr/local/apache/cgi-bin/$X
	rsync -ar --delete hgNearData /usr/local/apache/cgi-bin-$(USER)/

compile: $O $(MYLIBS)
	gcc -ggdb -o $X $O $(MYLIBS) $L
	strip $X
	chmod a+rx $X
