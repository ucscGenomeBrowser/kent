include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txPaperPfam.o

txPaperPfam: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txPaperPfam $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txPaperPfam${EXE}

clean:
	rm -f $O
