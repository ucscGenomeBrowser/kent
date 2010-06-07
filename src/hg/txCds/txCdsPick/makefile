include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsPick.o  

txCdsPick: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsPick $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsPick${EXE}

clean:
	rm -f $O
