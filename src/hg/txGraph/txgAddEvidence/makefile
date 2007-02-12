include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgAddEvidence.o

txgAddEvidence: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txgAddEvidence $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/txgAddEvidence${EXE}

test:
	txgAddEvidence input.txg input.bed esttt output.txg
	diff expected.txg output.txg

clean:
	rm -f $O
