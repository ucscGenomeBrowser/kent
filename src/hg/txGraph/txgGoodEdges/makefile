include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgGoodEdges.o

txgGoodEdges: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txgGoodEdges $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txgGoodEdges${EXE}

test:
	txgGoodEdges input.txg input.weights 3 good output.edges
	diff expected.edges output.edges

clean:
	rm -f $O
