kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneTest.o

visiGeneTest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/visiGeneTest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/visiGeneTest${EXE}

clean:
	rm -f $O
