kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sqlPerf.o

sqlPerf: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sqlPerf $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sqlPerf${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} -o sqlPerf $O ${MYLIBS} $L

clean:
	rm -f $O sqlPerf
