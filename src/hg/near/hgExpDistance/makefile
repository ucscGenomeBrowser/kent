include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgExpDistance.o

hgExpDistance: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgExpDistance $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgExpDistance${EXE}

clean::
	rm -f ${O} hgExpDistance
