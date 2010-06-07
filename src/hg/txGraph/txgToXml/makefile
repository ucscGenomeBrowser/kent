include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgToXml.o

txgToXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txgToXml $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txgToXml${EXE}

test:
	txgToXml -verbose=0 input.txg output.xml
	diff expected.xml output.xml

clean:
	rm -f $O
