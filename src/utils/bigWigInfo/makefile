include ../../inc/common.mk

L += ${SOCKETLIB} -lm -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = bigWigInfo
O = bigWigInfo.o

bigWigInfo: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

clean:
	rm -f $O ${A}${EXE}
