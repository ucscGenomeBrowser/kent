include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sufaFind.o

sufaFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sufaFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sufaFind${EXE}

clean:
	rm -f $O
