kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = encode2BedDoctor
O = encode2BedDoctor.o

encode2BedDoctor: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}
	scp ${DESTDIR}${BINDIR}/${A}${EXE} encodedcc.sdsc.edu:bin/x86_64/

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

test:
	encode2BedDoctor test.bed fixed.bed
	cmp fixed.bed expected.bed

clean::
	rm -f ${A}${EXE} ${O}

