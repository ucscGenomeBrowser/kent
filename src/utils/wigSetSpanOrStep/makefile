kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = wigSetSpanOrStep
O = wigSetSpanOrStep.o

wigSetSpanOrStep: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	wigSetSpanOrStep span=50 inVar.wig outVar.wig
	diff expectedVar.wig outVar.wig
	wigSetSpanOrStep step=5 inFixed.wig outFixed.wig
	diff expectedFixed.wig outFixed.wig
