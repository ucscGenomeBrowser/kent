include ../../../inc/common.mk

L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rangeGraph.o rgi.o

rangeGraph: $O 
	gcc -o rangeGraph $O $(MYLIBS) $L

