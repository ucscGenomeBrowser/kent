include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txCdsPredict.o orfInfo.o

txCdsPredict: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsPredict $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsPredict${EXE}

test:
	txCdsPredict in.fa out.cds
	cmp expected.cds out.cds

clean:
	rm -f $O
