kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToBioImage.o

knownToBioImage: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/knownToBioImage $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/knownToBioImage${EXE}

clean:
	rm -f $O
