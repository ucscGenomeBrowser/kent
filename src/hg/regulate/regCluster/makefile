kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = regCluster
O = regCluster.o 

regCluster: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A} ${O}

test:
	cd tests/needBreak; regCluster -weakLevel=0.3 in.tab out.cluster out.bed
	diff tests/needBreak/expected.cluster tests/needBreak/out.cluster
	diff tests/needBreak/expected.bed tests/needBreak/out.bed
