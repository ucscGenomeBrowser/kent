include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeChromGraph.o

fakeChromGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeChromGraph $O ${MYLIBS} $L
	strip ${BINDIR}/fakeChromGraph

clean:
	rm -f $O
