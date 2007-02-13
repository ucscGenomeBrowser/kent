include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txWalk.o

txWalk: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txWalk $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txWalk${EXE}

test:
	txWalk input.txg input.weights 3 output.bed

clean:
	rm -f $O
