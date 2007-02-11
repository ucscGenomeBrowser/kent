include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = agxToTxg.o

agxToTxg: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/agxToTxg $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/agxToTxg${EXE}

test:
	agxToTxg input.agx output.txg
	diff expected.txg output.txg

clean:
	rm -f $O
