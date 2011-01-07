include ../../inc/common.mk

L += ${SOCKETLIB} -lm -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

O = bigWigInfo.o

bigWigInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/bigWigInfo $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/bigWigInfo${EXE}

clean:
	rm -f $O
