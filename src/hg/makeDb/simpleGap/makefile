include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

CFLAGS= -ggdb
O = simpleGap.o

simpleGap: $O ${MYLIBS}
	${CC} -ggdb ${COPT} -o ${HOME}/bin/${MACHTYPE}/simpleGap $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/simpleGap

clean:
	rm -f $O
