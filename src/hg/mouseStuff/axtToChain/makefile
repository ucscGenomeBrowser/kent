include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = axtToChain
O = ${A}.o

strip: ${BINDIR}/${A}
	${STRIP} ${BINDIR}/${A}${EXE}

compile: ${BINDIR}/${A}

${BINDIR}/${A}: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} ${L}

clean::
	rm -f ${O}
