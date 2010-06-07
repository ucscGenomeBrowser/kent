include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txCdsSvmInput.o 

txCdsSvmInput: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txCdsSvmInput $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txCdsSvmInput${EXE}

test:
	txCdsSvmInput in.bed in.tce in.ortho in.info out.lst out.vector
	diff expected.lst out.lst
	diff expected.vector out.vector

clean:
	rm -f $O
