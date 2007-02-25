include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txInfoAssemble.o

txInfoAssemble: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txInfoAssemble $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txInfoAssemble${EXE}

clean:
	rm -f $O
