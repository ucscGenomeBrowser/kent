include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsPredict.o

txCdsPredict: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsPredict $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsPredict${EXE}

test:
	txCdsPredict in.fa out.cds
	diff expected.cds out.cds
	txCdsPredict in.fa -nmd=in.bed outNmd.cds
	diff expectedNmd.cds outNmd.cds
	txCdsPredict in.fa -maf=in.maf outMaf.cds
	diff expectedMaf.cds outMaf.cds

clean:
	rm -f $O
