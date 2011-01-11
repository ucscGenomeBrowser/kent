include ../../inc/common.mk

L += ${SOCKETLIB} -lm -lz
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkweb.a

A = bedClip
O = bedClip.o
B = ${DESTDIR}${BINDIR}/${A}${EXE}

bedClip: ${O} ${MYLIBS}
	@${MKDIR} `dirname $B`
	${CC} ${COPT} -o ${B} $O ${MYLIBS} $L
	${STRIP} ${B}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A} ${O}
