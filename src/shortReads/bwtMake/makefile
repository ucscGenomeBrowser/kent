include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bwtMake.o

bwtMake: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bwtMake $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bwtMake${EXE}

clean:
	rm -f $O
