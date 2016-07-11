kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

CFLAGS= -ggdb
O = simpleGap.o

simpleGap: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/simpleGap $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/simpleGap${EXE}

clean:
	rm -f $O
