include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsBadBed.o

txCdsBadBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsBadBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsBadBed${EXE}

clean:
	rm -f $O
