
include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/libSpliceGraph.a  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
GNUSL = /cluster/home/sugnet/local

% : %.c
	gcc -Wimplicit -ggdb -I../inc -I../../inc  -I../../../inc -I$(GNUSL)/include -L$(GNUSL)/lib -o $@ $(addsuffix .c,$@) $(MYLIBS) $(MYSQLLIBS) $(GNUSL)/lib/libgsl.a $(GNUSL)/lib/libgslcblas.a -lm -lz 
	cp $@ $(HOME)/bin/$(ARCHTYPE)

%Test: %Test.c 
	gcc -Wimplicit -ggdb  -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)


O =  gcPresAbs junctionSets agxStartsEnds bedsSpliceCheck probeMatcher seqForExonProbes mergeAffyProbes merge1lqAndCels makeRFiles

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib; make
