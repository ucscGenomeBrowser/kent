include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a ${L}

A = faFrag
O = faFrag.o

faFrag: ${O}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O}
