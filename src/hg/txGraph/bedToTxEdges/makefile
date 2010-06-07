include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = bedToTxEdges.o

bedToTxEdges: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedToTxEdges $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedToTxEdges${EXE}

test:
	bedToTxEdges tests/simple.in tests/simple.out
	diff tests/simple.expected tests/simple.out
	bedToTxEdges tests/blocked.in tests/blocked.out
	diff tests/blocked.expected tests/blocked.out

clean:
	rm -f $O
