include ../../inc/common.mk

L += -lm -lz ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

O = bedGraphToBigWig.o

bedGraphToBigWig: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/bedGraphToBigWig $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/bedGraphToBigWig${EXE}

clean:
	rm -f $O
