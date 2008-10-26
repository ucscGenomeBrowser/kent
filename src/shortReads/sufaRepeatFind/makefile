include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sufaRepeatFind.o

sufaRepeatFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sufaRepeatFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sufaRepeatFind${EXE}

clean:
	rm -f $O
