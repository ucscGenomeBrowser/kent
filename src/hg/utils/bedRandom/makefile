kentSrc = ../../..
include ../../../inc/common.mk

MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = bedRandom
O = bedRandom.o

bedRandom: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
