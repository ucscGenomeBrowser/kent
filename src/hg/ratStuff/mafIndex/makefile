kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafIndex.o

mafIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mafIndex $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/mafIndex${EXE}

clean:
	rm -f $O
