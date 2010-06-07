include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txAbFragFind.o

txAbFragFind: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txAbFragFind $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txAbFragFind${EXE}

clean:
	rm -f $O
