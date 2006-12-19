include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = minEachRow.o

minEachRow: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/minEachRow $O ${MYLIBS} $L
	strip ${BINDIR}/minEachRow

clean:
	rm -f $O
