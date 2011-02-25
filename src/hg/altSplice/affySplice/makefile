
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/libSpliceGraph.a  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
GNUSL = /cluster/home/sugnet/local

% : %.c
	$(CC) $(COPT) -I../inc -I../../inc  -I../../../inc -I$(GNUSL)/include -L$(GNUSL)/lib -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L} $(MYSQLLIBS) $(GNUSL)/lib/libgsl.a $(GNUSL)/lib/libgslcblas.a -lm -lz 
	cp $@ ${BINDIR}

O =  altHtmlPages bedOverlap gcPresAbs junctionSets agxStartsEnds \
     bedsSpliceCheck probeMatcher seqForExonProbes mergeAffyProbes \
      merge1lqAndCels makeRFiles rnaBindingClusterCorr calcSpliceDistance  \
     altProbes eseScorer spliceProbeVis dataMatrixToSql dataMatrixToSoft

exe: $(O)

test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
