
include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../lib/${MACHTYPE}/libSpliceGraph.a 

% : %.c
	${CC} ${COPT} -I../inc -I../../inc  -I../../../inc  -o $@ ${addsuffix .c,$@} ${MYLIBS} ${MYSQLLIBS} -lm -lz
	cp $@ ${HOME}/bin/${MACHTYPE}

%Test: %Test.c 
	${CC} ${COPT} -gp  -I../../inc -I../../../inc  -o $@ ${addsuffix .c,$@} ${MYLIBS} ${L}


O =  sequenceForBed  altAnalysis \
	altSplice orthoSplice agxToBed altSummary bedInGraph \
	consForBed altPaths bedToExons genePredToPsl
# genePredToBed genePredToPsl
TESTO = test_geneGraph.o

TESTS = altGraphXMemTest geneGraphTest 

exe: ${O}


test: ${TESTS}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
