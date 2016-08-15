kentSrc = ../../..
include	../../../inc/common.mk

OPT = ${COPT} ${HG_WARN} ${HG_DEFS}

MYLIBDIR = ../../../lib/${MACHTYPE}
LIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ./${MACHTYPE}/libSpliceGraph.a

%Test: %Test.c 
	${CC} ${OPT} -I../inc -I../../inc -I../../../inc -I${MYSQLINC}  -o $@ ${addsuffix .c,$@} ${LIBS} 

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = spliceGraph.o exonGraph.o heap.o altSpliceSite.o pfamDat.o affyProbe.o altProbe.o dMatrix.o \
    splice.o mouseAPSetEventMap.o

TESTS = heapTest

./${MACHTYPE}/libSpliceGraph.a: ${O} 
	@[ -d ${MACHTYPE} ] || ${MKDIR} ${MACHTYPE}	
	ar rcu ./${MACHTYPE}/libSpliceGraph.a ${O}
	ranlib ./${MACHTYPE}/libSpliceGraph.a

clean:
	rm -f ${O};
	rm -f ${TESTS}
	rm -f ./${MACHTYPE}/libSpliceGraph.a
	rm -fr ./${MACHTYPE}

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tests: 	${TESTS} ${O}

