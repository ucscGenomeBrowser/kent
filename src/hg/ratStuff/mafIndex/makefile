include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafIndex.o

mafIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mafIndex $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mafIndex${EXE}

clean:
	rm -f $O
