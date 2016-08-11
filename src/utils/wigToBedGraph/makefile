kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = wigToBedGraph
O = wigToBedGraph.o

wigToBedGraph: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A} ${O}

test:
	wigToBedGraph testFixed.wig tmp.bedGraph
	diff testFixed.bedGraph tmp.bedGraph
	wigToBedGraph testVar.wig tmp.bedGraph
	diff testVar.bedGraph tmp.bedGraph
