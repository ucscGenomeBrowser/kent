include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneCanonical.o

txGeneCanonical: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneCanonical $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneCanonical${EXE}

clean:
	rm -f $O
