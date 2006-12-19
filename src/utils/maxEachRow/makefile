include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = maxEachRow.o

maxEachRow: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/maxEachRow $O ${MYLIBS} $L
	strip ${BINDIR}/maxEachRow

clean:
	rm -f $O
