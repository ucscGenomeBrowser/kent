include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToGenePix.o

knownToGenePix: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/knownToGenePix $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/knownToGenePix${EXE}

clean:
	rm -f $O
