include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsWeed.o

txCdsWeed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsWeed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsWeed${EXE}

clean:
	rm -f $O
