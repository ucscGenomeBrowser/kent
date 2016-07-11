kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testIndex.o

testIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testIndex $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testIndex${EXE}

clean:
	rm -f $O
