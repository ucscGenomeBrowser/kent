include ../../../inc/common.mk

CFLAGS= -ggdb

L =  ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a   ${MYLIBDIR}/jkweb.a

O = mafGene.o

mafGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafGene $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/mafGene${EXE}

clean:
	rm -f $O
