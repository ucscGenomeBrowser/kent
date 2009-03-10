include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsEvFromProtein.o

txCdsEvFromProtein: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsEvFromProtein $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsEvFromProtein${EXE}

test:
	txCdsEvFromProtein inProtein.fa in.psl inTx.fa output.tce -unmapped=output.unmapped -dodgeStop=3
	diff expected.tce output.tce
	diff expected.unmapped output.unmapped

clean:
	rm -f $O
