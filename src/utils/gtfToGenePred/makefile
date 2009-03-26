include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ${L}
HG_INC=-I../../inc -I../../hg/inc

A = gtfToGenePred
O = gtfToGenePred.o

gtfToGenePred: $O ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

test:
	(cd tests && ${MAKE} test)

clean::
	rm -f ${O}
	(cd tests && ${MAKE} clean)
