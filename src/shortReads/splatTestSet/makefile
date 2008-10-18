include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = splatTestSet.o

splatTestSet: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/splatTestSet $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/splatTestSet${EXE}

clean:
	rm -f $O
