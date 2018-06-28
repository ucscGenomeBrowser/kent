kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = wigAsciiAverageOverBed.o

wigAsciiAverageOverBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigAsciiAverageOverBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigAsciiAverageOverBed${EXE}

clean:
	rm -f $O
