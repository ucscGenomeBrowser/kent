include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = splat.o extend.o output.o over.o

splat: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splat${EXE}

clean:
	rm -f $O
