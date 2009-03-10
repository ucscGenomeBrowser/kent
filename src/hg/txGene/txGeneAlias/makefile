include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneAlias.o

txGeneAlias: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneAlias $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneAlias${EXE}

clean:
	rm -f $O
