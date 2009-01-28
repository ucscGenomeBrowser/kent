include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wigToBigWig.o

wigToBigWig: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigToBigWig $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigToBigWig${EXE}

clean:
	rm -f $O
