include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sqlPerf.o

sqlPerf: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/sqlPerf $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/sqlPerf${EXE}

compile: $O ${MYLIBS}
	${CC} ${COPT} -o sqlPerf $O ${MYLIBS} $L

clean:
	rm -f $O sqlPerf
