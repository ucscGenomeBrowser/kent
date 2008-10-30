include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = itsaFind.o

itsaFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/itsaFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/itsaFind${EXE}

clean:
	rm -f $O
