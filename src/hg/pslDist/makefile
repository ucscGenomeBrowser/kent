include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = pslDist.o

pslDist: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslDist $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/pslDist

clean:
	rm -f $O
