include ../../inc/common.mk

L += -lm -lz ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = bedPileUps
O = bedPileUps.o

bedToBigBed: $O ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

clean::
	rm -f ${O} ${A}
	rm -fr tests/output

test:
	@rm -fr tests/output
	@${MKDIR} tests/output
	-${A} tests/input/foo.bed &> tests/output/foo.bed.txt
	diff tests/expected/foo.bed.txt tests/output/foo.bed.txt
	-${A} tests/input/foo2.bed &> tests/output/foo2.bed.txt
	diff tests/expected/foo2.bed.txt tests/output/foo2.bed.txt
	${A} tests/input/foo3.bed &> tests/output/foo3.bed.txt
	diff tests/expected/foo3.bed.txt tests/output/foo3.bed.txt
	${A} tests/input/foo4.bed &> tests/output/foo4.bed.txt
	diff tests/expected/foo4.bed.txt tests/output/foo4.bed.txt
	rm -fr tests/output
