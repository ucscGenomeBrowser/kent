include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsToGene.o

txCdsToGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsToGene $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsToGene${EXE}

test:
	txCdsToGene in.bed in.fa in.tce output.gtf output.fa
	diff expected.fa output.fa
	diff expected.gtf output.gtf

clean:
	rm -f $O
