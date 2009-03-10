include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txOrtho.o

txOrtho: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txOrtho $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txOrtho${EXE}

test:
	txOrtho input.txg input.chain input.net inOrtho.txg output.edges -verbose=0
	diff expected.edges output.edges

clean:
	rm -f $O

