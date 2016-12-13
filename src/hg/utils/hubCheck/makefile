kentSrc = ../../..
include ../../../inc/common.mk

L += ${HALLIBS} ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hubCheck
O = hubCheck.o

hubCheck: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

test::
	(cd tests && ${MAKE} test) 2> /dev/null

clean::
	rm -f ${A}${EXE} ${O}
