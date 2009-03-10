include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = knownToHprd
O = ${A}.o

knownToHprd: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

test: ${BINDIR}/${A}${EXE}
	knownToHprd -verbose=2 -keepTab hg18 \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/HPRD_ID_MAPPINGS.txt

clean:
	rm -f $O
