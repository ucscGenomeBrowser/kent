include ../../../inc/common.mk

L = -lm ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = clusterPsl.o addCluster.o writeCluster.o

clusterPsl: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/clusterPsl $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/clusterPsl${EXE}

clean:
	rm -f $O
