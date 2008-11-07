include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bwtReverse.o

bwtReverse: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bwtReverse $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bwtReverse${EXE}

clean:
	rm -f $O
