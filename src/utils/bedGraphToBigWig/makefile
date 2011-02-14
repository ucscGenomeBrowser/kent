include ../../inc/common.mk

L += -lm -lz ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = bedGraphToBigWig
O = bedGraphToBigWig.o

bedGraphToBigWig: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/bedGraphToBigWig $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/bedGraphToBigWig${EXE}

compile: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

clean:
	rm -f ${O} ${A}${EXE}
