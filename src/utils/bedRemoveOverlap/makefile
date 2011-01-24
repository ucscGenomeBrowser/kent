include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = bedRemoveOverlap
O = bedRemoveOverlap.o

bedRemoveOverlap: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	bedRemoveOverlap empty.bed empty.out
	diff empty.out empty.expected
	bedRemoveOverlap one.bed one.out
	diff one.out one.expected
	bedRemoveOverlap test4.bed test4.out
	diff test4.out test4.expected
	bedRemoveOverlap threeLast.bed threeLast.out
	diff threeLast.out threeLast.expected
	bedRemoveOverlap twoFirst.bed twoFirst.out
	diff twoFirst.out twoFirst.expected
	bedRemoveOverlap twoSecond.bed twoSecond.out
	diff twoSecond.out twoSecond.expected

