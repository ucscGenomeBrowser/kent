kentSrc = ../..
include ../../inc/common.mk

L=-lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = clusterClone.o
A = clusterClone

${A}: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

clean:
	rm -f $O

