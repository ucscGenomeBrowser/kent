include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bedGraphToBigWig.o

bedGraphToBigWig: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedGraphToBigWig $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedGraphToBigWig${EXE}

clean:
	rm -f $O
