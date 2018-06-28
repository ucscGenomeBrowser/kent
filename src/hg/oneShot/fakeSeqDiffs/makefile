kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fakeSeqDiffs.o

fakeSeqDiffs: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fakeSeqDiffs $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fakeSeqDiffs${EXE}

clean:
	rm -f $O
