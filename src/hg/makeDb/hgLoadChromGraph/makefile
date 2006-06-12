include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadChromGraph.o

hgLoadChromGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgLoadChromGraph $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgLoadChromGraph

clean:
	rm -f $O
