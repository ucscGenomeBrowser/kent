COPT= -g
include ../../../inc/common.mk

L += -lm  $(MYSQLLIBS)

MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS = ../lib/${MACHTYPE}/encode.a  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = encodeRenameObj
O = ${A}.o

metaCheck: ${O} ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
#	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O}
	${CC} ${COPT} -o ${A}${EXE} ${O} ${MYLIBS} $L

clean::
	rm -f ${A}${EXE} ${O}
