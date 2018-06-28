kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneLoad.o

visiGeneLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/visiGeneLoad $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/visiGeneLoad${EXE}

clean:
	rm -f $O
