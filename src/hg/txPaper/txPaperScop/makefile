kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txPaperScop.o

txPaperScop: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txPaperScop $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txPaperScop${EXE}

clean:
	rm -f $O
