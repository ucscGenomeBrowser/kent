include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

XINC=-I../../hg/inc

O = bdfToGem.o

A = bdfToGem

all::	${A}

$A: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}

clean:
	rm -f ${HOME}/bin/${MACHTYPE}/${A} $O
