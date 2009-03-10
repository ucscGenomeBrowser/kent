include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsCluster.o

txCdsCluster: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsCluster $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsCluster${EXE}

test:
	txCdsCluster input.bed output.clusters
	diff expected.clusters output.clusters

clean:
	rm -f $O
