include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splatCheck1.o

splatCheck1: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splatCheck1 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splatCheck1${EXE}

clean:
	rm -f $O
