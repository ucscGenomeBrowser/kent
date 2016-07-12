kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = replaceTextBetween
O = replaceTextBetween.o

replaceTextBetween: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	replaceTextBetween 'is a ' ' test' testOuter testInner > testOut
	diff testExpected testOut
	replaceTextBetween -withEnds 'is a ' ' test' testOuter testInner > testOutWithEnds
	diff testExpectedWithEnds testOutWithEnds
