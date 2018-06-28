kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txPaperPfam.o

txPaperPfam: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txPaperPfam $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txPaperPfam${EXE}

clean:
	rm -f $O
