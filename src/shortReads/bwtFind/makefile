include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bwtFind.o

bwtFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bwtFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bwtFind${EXE}

clean:
	rm -f $O
