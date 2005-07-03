include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = pslCluster.o

pslCluster: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslCluster $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/pslCluster${EXE}

pslMaxMap: pslMaxMap.o ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslMaxMap pslMaxMap.o ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/pslCluster${EXE}

clean:
	rm -f $O
