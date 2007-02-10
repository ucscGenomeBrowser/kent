include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txgToXml.o

txgToXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/txgToXml $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/txgToXml${EXE}

clean:
	rm -f $O
