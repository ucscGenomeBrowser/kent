kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = addTopoToPrimers.o

addTopoToPrimers: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/addTopoToPrimers $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/addTopoToPrimers${EXE}

clean:
	rm -f $O
