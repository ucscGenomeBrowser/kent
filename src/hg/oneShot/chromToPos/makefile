kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = chromToPos.o

chromToPos: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/chromToPos $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/chromToPos${EXE}

clean:
	rm -f $O
