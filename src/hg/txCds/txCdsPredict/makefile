include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsPredict.o orfInfo.o

txCdsPredict: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsPredict $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsPredict${EXE}

test:
	txCdsPredict in.fa out.cds
	diff expected.cds out.cds
	txCdsPredict in.fa -nmd=in.bed outNmd.cds
	diff expectedNmd.cds outNmd.cds
	txCdsPredict in.fa -maf=in.maf outMaf.cds
	diff expectedMaf.cds outMaf.cds

clean:
	rm -f $O
