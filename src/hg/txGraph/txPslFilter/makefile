include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txPslFilter.o

txPslFilter: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txPslFilter $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txPslFilter${EXE}

clean:
	rm -f $O
