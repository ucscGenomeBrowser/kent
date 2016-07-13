kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = countLines.o

countLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/countLines $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/countLines${EXE}

clean:
	rm -f $O
