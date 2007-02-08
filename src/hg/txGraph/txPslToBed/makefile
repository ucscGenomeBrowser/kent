include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txPslToBed.o

txPslToBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txPslToBed $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txPslToBed${EXE}

clean:
	rm -f $O
