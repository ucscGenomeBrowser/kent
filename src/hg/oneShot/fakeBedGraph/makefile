kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeBedGraph.o

fakeBedGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeBedGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fakeBedGraph${EXE}

clean:
	rm -f $O
