include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txCdsRaExceptions.o

txCdsRaExceptions: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsRaExceptions $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsRaExceptions${EXE}

clean:
	rm -f $O
