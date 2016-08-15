kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeChromGraph.o

fakeChromGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeChromGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fakeChromGraph${EXE}

clean:
	rm -f $O
