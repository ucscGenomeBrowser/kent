include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgAnalyze.o

txgAnalyze: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txgAnalyze $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txgAnalyze${EXE}

clean:
	rm -f $O
