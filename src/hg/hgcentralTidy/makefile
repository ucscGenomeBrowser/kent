kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgcentralTidy.o

hgcentralTidy: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgcentralTidy $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgcentralTidy${EXE}

clean:
	rm -f $O
