include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = pslCluster.o

pslCluster: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslCluster $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/pslCluster

pslMaxMap: pslMaxMap.o ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslMaxMap pslMaxMap.o ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/pslCluster

clean:
	rm -f $O
