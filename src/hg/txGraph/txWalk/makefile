include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txWalk.o

txWalk: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txWalk $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txWalk${EXE}

test:
	txWalk input.txg input.weights 3 output.bed -evidence=output.ev
	diff expected.bed output.bed
	diff expected.ev output.ev

clean:
	rm -f $O
