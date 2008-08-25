include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneExplainUpdate2.o

txGeneExplainUpdate2: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneExplainUpdate2 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneExplainUpdate2${EXE}

clean:
	rm -f $O
