include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txGeneAccession.o

txGeneAccession: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txGeneAccession $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txGeneAccession${EXE}

test:
	txGeneAccession inOld.bed in.id inNew.bed output.tab outOldToNew.tab -test
	diff expected.tab output.tab

clean:
	rm -f $O
