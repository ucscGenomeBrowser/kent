include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslSomeRecords.o

pslSomeRecords: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslSomeRecords $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pslSomeRecords

clean:
	rm -f $O
