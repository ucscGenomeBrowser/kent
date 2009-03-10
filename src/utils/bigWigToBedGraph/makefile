include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigWigToBedGraph.o 

bigWigToBedGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigWigToBedGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigWigToBedGraph${EXE}

clean:
	rm -f $O
