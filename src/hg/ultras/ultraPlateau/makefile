kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = ultraPlateau.o

ultraPlateau: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/ultraPlateau $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/ultraPlateau${EXE}

clean:
	rm -f $O
