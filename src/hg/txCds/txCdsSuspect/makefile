include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsSuspect.o

txCdsSuspect: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsSuspect $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsSuspect${EXE}

test:
	txCdsSuspect in.bed in.txg in.cluster in.info out.suspect -niceProt=out.nice
	diff expected.suspect out.suspect
	diff expected.nice out.nice

clean:
	rm -f $O
