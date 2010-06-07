include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgTrim.o

txgTrim: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txgTrim $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txgTrim${EXE}

test:
	txgTrim input.txg input.weights 3.0 output.txg
	diff expected.txg output.txg
	txgTrim output.txg input.weights 3.0 output2.txg
	diff expected.txg output2.txg

clean:
	rm -f $O
