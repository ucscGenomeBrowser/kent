include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

MYLIBDIR = ../../lib/${MACHTYPE}
L += ${MYSQLLIBS} -lm
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = genePredHisto
O = ${A}.o

genePredHisto: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L

compile:
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} $L

test: compile
	(cd tests && ${MAKE} test)
	rm -fr tests/output ${A}${EXE} ${A}.o

clean::
	rm -fr tests/output ${A}${EXE} ${A}.o
