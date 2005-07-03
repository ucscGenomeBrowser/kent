include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = addTopoToPrimers.o

addTopoToPrimers: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/addTopoToPrimers $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/addTopoToPrimers${EXE}

clean:
	rm -f $O
