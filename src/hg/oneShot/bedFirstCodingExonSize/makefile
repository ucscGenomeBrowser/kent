kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bedFirstCodingExonSize.o

bedFirstCodingExonSize: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bedFirstCodingExonSize $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bedFirstCodingExonSize${EXE}

clean:
	rm -f $O
