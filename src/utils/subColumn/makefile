include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = subColumn.o

subColumn: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/subColumn $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/subColumn${EXE}

clean:
	rm -f $O
