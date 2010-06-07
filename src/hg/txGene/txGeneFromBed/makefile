include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneFromBed.o

txGeneFromBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneFromBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneFromBed${EXE}

clean:
	rm -f $O
