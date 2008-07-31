include ../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = sqlToXml.o

sqlToXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sqlToXml $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sqlToXml${EXE}

clean:
	rm -f $O
	rm -f test/*/out.*

test::
	./doTest
