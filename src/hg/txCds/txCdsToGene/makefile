include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsToGene.o

txCdsToGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsToGene $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsToGene${EXE}

test:
	txCdsToGene in.bed in.fa in.tce output.gtf output.fa -bedOut=output.bed
	diff expected.fa output.fa
	diff expected.gtf output.gtf
	diff expected.bed output.bed

clean:
	rm -f $O
