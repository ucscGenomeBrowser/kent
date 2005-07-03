include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = genePixLoad.o

genePixLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/genePixLoad $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/genePixLoad${EXE}

clean:
	rm -f $O
