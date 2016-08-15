kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

targets: chainOneTwo pslOneTwo
O = pslOneTwo.o

chainOneTwo: ${MYLIBS} chainOneTwo.o 
	${CC} ${COPT} -o ${BINDIR}/chainOneTwo chainOneTwo.o ${MYLIBS} $L

pslOneTwo: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/pslOneTwo $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/pslOneTwo${EXE}

clean:
	rm -f $O
