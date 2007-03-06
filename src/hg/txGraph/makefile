include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txGraph.o

txGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGraph $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGraph${EXE}

clean:
	rm -f $O
