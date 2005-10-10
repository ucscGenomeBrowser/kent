include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToVisiGene.o

knownToGenePix: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/knownToVisiGene $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/knownToVisiGene${EXE}

clean:
	rm -f $O
