kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = segInfo.o

segInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/segInfo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/segInfo${EXE}

clean:
	rm -f $O
