
include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a 

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm -lz
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)


O =  altAnalysis altSplice orthoSplice agxToBed altSummary # spliceStats spliceVis

TESTO = test_geneGraph.o

TESTS = altGraphXMemTest geneGraphTest 

exe: $(O)


test: $(TESTS)

lib:
	cd ../../lib; make
