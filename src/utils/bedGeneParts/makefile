include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = bedGeneParts
O = bedGeneParts.o

bedGeneParts: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}

test:
	bedGeneParts firstExon test.in firstExon.out
	diff firstExon.expected firstExon.out
	bedGeneParts introns test.in introns.out
	diff introns.expected introns.out
	bedGeneParts promoter test.in promoter.out
	diff promoter.expected promoter.out
	bedGeneParts firstCodingSplice test.in firstCodingSplice.out
	diff firstCodingSplice.expected firstCodingSplice.out
