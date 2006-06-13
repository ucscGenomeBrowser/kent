include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = fakeChromGraph.o

fakeChromGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fakeChromGraph $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/fakeChromGraph

clean:
	rm -f $O
