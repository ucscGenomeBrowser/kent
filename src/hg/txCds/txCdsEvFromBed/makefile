include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = txCdsEvFromBed.o

txCdsEvFromBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsEvFromBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsEvFromBed${EXE}

clean:
	rm -f $O
