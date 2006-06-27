include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeBedGraph.o

fakeBedGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fakeBedGraph $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/fakeBedGraph

clean:
	rm -f $O
