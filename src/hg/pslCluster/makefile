kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
CFLAGS= -ggdb

O = pslCluster.o

pslCluster: $O ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${BINDIR}/pslCluster $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/pslCluster${EXE}

pslMaxMap: pslMaxMap.o ${MYLIBS}
	${CC} ${CFLAGS} ${COPT} -o ${BINDIR}/pslMaxMap pslMaxMap.o ${MYLIBS} $L
#	${STRIP} ${BINDIR}/pslCluster${EXE}

clean:
	rm -f $O
