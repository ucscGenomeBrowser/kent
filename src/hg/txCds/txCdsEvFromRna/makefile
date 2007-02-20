include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsEvFromRna.o

txCdsEvFromRna: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txCdsEvFromRna $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txCdsEvFromRna${EXE}

test:
	txCdsEvFromRna inRna.fa in.cds in.psl inTx.fa output.ev -refStatus=inRefStatus.tab -unmapped=output.unmapped
	diff expected.ev output.ev
	diff expected.unmapped output.unmapped

clean:
	rm -f $O
