include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bptForTwoBit.o

bptForTwoBit: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bptForTwoBit $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bptForTwoBit${EXE}

clean:
	rm -f $O
