include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsEvFromBorf.o

txCdsEvFromBorf: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsEvFromBorf $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsEvFromBorf${EXE}

test:
	txCdsEvFromBorf in.borf in.fa out.tce
	diff expected.tce out.tce

clean:
	rm -f $O
