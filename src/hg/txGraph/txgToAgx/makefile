include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgToAgx.o

txgToAgx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txgToAgx $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txgToAgx${EXE}

test:
	txgToAgx input.txg output.agx
	diff expected.agx output.agx

clean:
	rm -f $O
