include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a ${L}

A = bigBedSummary
O = bigBedSummary.o

bigBedSummary: ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A} $O ${MYLIBS}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}
