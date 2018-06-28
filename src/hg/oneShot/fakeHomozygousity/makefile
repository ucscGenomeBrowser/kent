kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fakeHomozygousity.o

fakeHomozygousity: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeHomozygousity $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fakeHomozygousity${EXE}

clean:
	rm -f $O
