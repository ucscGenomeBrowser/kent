include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txBedToTxg.o

txBedToTxg: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txBedToTxg $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txBedToTxg${EXE}

clean:
	rm -f $O
