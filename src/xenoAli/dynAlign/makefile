include ../../inc/common.mk

MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a
L += -lm


A = dynAlign
O = dynAlign.o 

$A: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} $O ${MYLIBS} ${L}

push:	
	mv ${AOUT} ${CGI_BIN}-${USER}/dynAlign.exe

clean::
	rm -f "{O}" "${AOUT}"
