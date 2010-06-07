include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneProtAndRna.o

txGeneProtAndRna: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneProtAndRna $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneProtAndRna${EXE}

clean:
	rm -f $O
