include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToVisiGene.o

knownToVisiGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/knownToVisiGene $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/knownToVisiGene${EXE}

clean:
	rm -f $O
