include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgExpDistance.o

hgExpDistance: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgExpDistance $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgExpDistance${EXE}

clean::
	rm -f ${O} hgExpDistance
