include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a ${L}

A = textHistogram
O = textHistogram.o

textHistogram: $O ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}
