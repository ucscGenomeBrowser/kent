include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

BINDIR = ${HOME}/bin/${MACHTYPE}
A = axtToChain
O = ${A}.o

strip: ${BINDIR}/${A}
	strip ${BINDIR}/${A}

compile: ${BINDIR}/${A}

${BINDIR}/${A}: $O ${MYLIBS}
	gcc -ggdb -o ${BINDIR}/${A} $O ${MYLIBS} ${L}

