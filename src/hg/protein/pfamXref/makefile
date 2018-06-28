kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = pfamXref
O = $A.o

$A: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$A $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/$A${EXE}

clean::
	rm -f ${O}
