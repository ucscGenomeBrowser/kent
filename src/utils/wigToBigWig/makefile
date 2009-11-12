include ../../inc/common.mk

L += -lm ${SOCKETLIB} -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = wigToBigWig
O = wigToBigWig.o

wigToBigWig: ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}

clean::
	rm -f ${O}
