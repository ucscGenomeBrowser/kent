include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = bedItemOverlapCount
O = bedItemOverlapCount.o

${A}: ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

test:	${A}
	cd tests && ./RunTest.sh

testVerbose:	${A}
	cd tests && ./RunTest.sh -verbose

compile: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A} $O ${MYLIBS} $L

clean::
	rm -f ${O} ${A}
