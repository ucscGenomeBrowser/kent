kentSrc = ../../..

include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm -lz
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../lib/${MACHTYPE}/libSpliceGraph.a 

% : %.c
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} \
            -o ${DESTDIR}${BINDIR}/$@ ${addsuffix .c,$@} ${MYLIBS} ${L}

%Test: %Test.c 
	${CC} ${COPT} -gp ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} \
            -o $@ ${addsuffix .c,$@} ${MYLIBS} ${L}


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
	cd tests; ${MAKE} clean
