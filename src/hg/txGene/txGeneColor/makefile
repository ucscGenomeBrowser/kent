include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 


O = txGeneColor.o

txGeneColor: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneColor $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneColor${EXE}

clean:
	rm -f $O
