include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = agxToTxg.o

agxToTxg: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/agxToTxg $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/agxToTxg${EXE}

test:
	agxToTxg input.agx output.txg
	diff expected.txg output.txg

clean:
	rm -f $O
