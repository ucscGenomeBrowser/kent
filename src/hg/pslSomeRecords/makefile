include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslSomeRecords.o

pslSomeRecords: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslSomeRecords $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/pslSomeRecords${EXE}

clean:
	rm -f $O
