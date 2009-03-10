include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgTablesTest.o

hgTablesTest: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${BINDIR}/hgTablesTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgTablesTest${EXE}

clean:
	rm -f $O
