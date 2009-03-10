include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneXref.o

txGeneXref: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneXref $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneXref${EXE}

clean:
	rm -f $O
