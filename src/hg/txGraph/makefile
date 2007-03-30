include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txGraph.o

txGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGraph${EXE}

clean:
	rm -f $O
