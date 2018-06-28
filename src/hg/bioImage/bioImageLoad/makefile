kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bioImageLoad.o

bioImageLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bioImageLoad $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bioImageLoad${EXE}

clean:
	rm -f $O
