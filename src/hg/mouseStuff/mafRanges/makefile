include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = mafRanges
O = ${A}.o

${A}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}
