kentSrc = ../../..

include ../../../inc/common.mk

L += ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../lib/${MACHTYPE}/libSpliceGraph.a 

# compile and link in separate steps, a one-step compile+link with -g
# on Mac OSX makes clang run dsymutil and leave a .dSYM directory in BINDIR
% : %.c

	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} \
	    -c -o $@.o ${addsuffix .c,$@}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/$@ $@.o ${MYLIBS} ${L}
	rm -f $@.o

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
	rm -f ${O} ${O:%=%.o}
	cd tests; ${MAKE} clean
