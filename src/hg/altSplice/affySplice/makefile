
include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/libSpliceGraph.a  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) -lm -lz
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)


O =  junctionSets agxStartsEnds bedsSpliceCheck probeMatcher seqForExonProbes mergeAffyProbes merge1lqAndCels makeRFiles

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib; make
