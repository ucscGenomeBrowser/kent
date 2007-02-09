include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgTrim.o

txgTrim: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txgTrim $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txgTrim${EXE}

test:
	txgTrim input.txg input.weights 2.0 output.txg

clean:
	rm -f $O
