include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneExplainUpdate1.o

txGeneExplainUpdate1: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneExplainUpdate1 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneExplainUpdate1${EXE}

clean:
	rm -f $O
