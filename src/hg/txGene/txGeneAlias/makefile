include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneAlias.o

txGeneAlias: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGeneAlias $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGeneAlias${EXE}

clean:
	rm -f $O
