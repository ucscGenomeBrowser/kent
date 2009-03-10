include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsRepick.o


txCdsRepick: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsRepick $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsRepick${EXE}

test:
	txCdsRepick in.bed in.txg in.cluster in.info in.tce out.tce out.pp
	diff expected.tce out.tce
	diff expected.pp out.pp

clean:
	rm -f $O
