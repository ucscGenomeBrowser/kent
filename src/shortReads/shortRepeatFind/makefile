include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = shortRepeatFind.o

shortRepeatFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/shortRepeatFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/shortRepeatFind${EXE}

clean:
	rm -f $O
