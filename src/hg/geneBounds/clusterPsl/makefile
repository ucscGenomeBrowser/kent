include ../../../inc/common.mk

L += -lm ${MYSQLLIBS} 
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = clusterPsl.o

clusterPsl: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/clusterPsl $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/clusterPsl${EXE}

test:	clusterPsl
	clusterPsl -verbose=0 input.psl output.cluster
	diff expected.cluster output.cluster

clean:
	rm -f ${O}
