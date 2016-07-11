kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testBb.o

testBb: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/testBb $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/testBb${EXE}

clean:
	rm -f $O
