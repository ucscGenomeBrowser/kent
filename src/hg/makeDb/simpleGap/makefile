include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

CFLAGS= -ggdb
O = simpleGap.o

simpleGap: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/simpleGap $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/simpleGap${EXE}

clean:
	rm -f $O
