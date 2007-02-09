include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txOrtho.o

txOrtho: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txOrtho $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txOrtho${EXE}

clean:
	rm -f $O
