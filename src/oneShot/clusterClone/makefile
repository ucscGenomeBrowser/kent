include ../../inc/common.mk

L=-lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = clusterClone.o
A = clusterClone

${A}: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/${A}

clean:
	rm -f $O

