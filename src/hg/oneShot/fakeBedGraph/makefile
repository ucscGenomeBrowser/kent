include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeBedGraph.o

fakeBedGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeBedGraph $O ${MYLIBS} $L
	strip ${BINDIR}/fakeBedGraph

clean:
	rm -f $O
