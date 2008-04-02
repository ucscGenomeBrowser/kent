HG_WARN = -Wall 
include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txBedToGraph.o makeGraph.o

txBedToGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txBedToGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txBedToGraph${EXE}

test:
	txBedToGraph -verbose=0 inRef.bed refSeq input.bed mrna output.txg 
	diff expected.txg output.txg

clean:
	rm -f $O
