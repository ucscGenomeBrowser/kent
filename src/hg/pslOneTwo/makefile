include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

targets: chainOneTwo pslOneTwo
O = pslOneTwo.o

chainOneTwo: ${MYLIBS} chainOneTwo.o 
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/chainOneTwo chainOneTwo.o ${MYLIBS} $L

pslOneTwo: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslOneTwo $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/pslOneTwo

clean:
	rm -f $O
