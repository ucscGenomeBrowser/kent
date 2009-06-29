include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bigWigInfo.o

bigWigInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigWigInfo $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigWigInfo${EXE}

clean:
	rm -f $O
