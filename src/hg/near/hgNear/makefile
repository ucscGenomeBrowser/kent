include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
X = hgNear

O = $X.o advFilter.o association.o configure.o examples.o expRatio.o getSeq.o \
	go.o gifLabel.o knownGene.o order.o pfam.o search.o userSettings.o

my: compile
	mv $X /usr/local/apache/cgi-bin-$(USER)/$X
	chmod -R a+r hgNearData
	chmod a+rx hgNearData
	find hgNearData -type d -exec /bin/chmod -f a+rx {} \;
	rsync -ar --delete hgNearData /usr/local/apache/cgi-bin-$(USER)/

alpha: compile
	mv $X /usr/local/apache/cgi-bin/$X
	rsync -ar --delete hgNearData /usr/local/apache/cgi-bin/

compile: $O $(MYLIBS)
	gcc -ggdb -o $X $O $(MYLIBS) $L
#	strip $X
	chmod a+rx $X
