kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgPutBack.o

kgPutBack: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/kgPutBack $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/kgPutBack${EXE}

clean::
	rm -f ${O}
