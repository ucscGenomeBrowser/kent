include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

A = encode2CopyManifest
O = encode2CopyManifest.o

encode2CopyManifest: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
