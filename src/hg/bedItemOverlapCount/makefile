include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = bedItemOverlapCount
O = bedItemOverlapCount.o

${A}: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

test:	${A}
	cd tests && ./RunTest.sh

testVerbose:	${A}
	cd tests && ./RunTest.sh -verbose

compile: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

clean::
	rm -f ${O} ${A}
