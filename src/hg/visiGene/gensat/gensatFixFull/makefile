kentSrc = ../../../..
include ../../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gensatFixFull.o

gensatFixFull: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatFixFull $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gensatFixFull${EXE}

clean:
	rm -f $O
