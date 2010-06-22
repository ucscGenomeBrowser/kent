include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgAddLiftOverChain.o
A = hgAddLiftOverChain

hgAddLiftOverChain: $O ${MYLIBS}
	${CC} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}
