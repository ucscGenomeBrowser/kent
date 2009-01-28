include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigWigSummary.o

bigWigSummary: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigWigSummary $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigWigSummary${EXE}

clean:
	rm -f $O
