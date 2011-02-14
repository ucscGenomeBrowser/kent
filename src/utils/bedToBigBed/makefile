include ../../inc/common.mk

L += -lm -lz ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = bedToBigBed
O = bedToBigBed.o

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
	${A} -as=itemRgb.as itemRgb.bed chrom.sizes tests/output/itemRgb.bb 2> /dev/null
	bigBedToBed tests/output/itemRgb.bb test_itemRgb.bed
	diff itemRgb.bed test_itemRgb.bed
	rm -fr tests/output test_itemRgb.bed
