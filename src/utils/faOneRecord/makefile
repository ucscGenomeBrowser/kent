include ../../inc/common.mk

MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = faOneRecord
O = faOneRecord.o

faOneRecord: ${O}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}
